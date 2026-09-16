import Darwin
import Foundation
import UIKit

// Screen 2: just a spinner while the pipeline runs — spawn the helper,
// exploit, hand off kernel addresses, elevate the helper, confirm it.
// A clean failure shows one line plus Back; a panic returns via relaunch
// routing (Start screen), never through here.
final class LoadingViewController: UIViewController {
    private let spinner: UIActivityIndicatorView = {
        if #available(iOS 13.0, *) {
            return UIActivityIndicatorView(style: .large)
        } else {
            return UIActivityIndicatorView(style: .whiteLarge)
        }
    }()

    private let errorLabel = UILabel()
    private let backButton = UIButton(type: .system)
    private let statusLabel = UILabel()
    private var ran = false
    private var stage = ""
    private var stageTimer: Timer?

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .white
        spinner.color = .gray
        spinner.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(spinner)

        errorLabel.font = .systemFont(ofSize: 14)
        errorLabel.textColor = .darkGray
        errorLabel.textAlignment = .center
        errorLabel.numberOfLines = 0
        errorLabel.isHidden = true
        errorLabel.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(errorLabel)

        backButton.setTitle("Back", for: .normal)
        backButton.titleLabel?.font = .systemFont(ofSize: 17)
        backButton.isHidden = true
        backButton.translatesAutoresizingMaskIntoConstraints = false
        backButton.addTarget(self, action: #selector(goBack), for: .touchUpInside)
        view.addSubview(backButton)

        statusLabel.font = .systemFont(ofSize: 13)
        statusLabel.textColor = .darkGray
        statusLabel.textAlignment = .center
        statusLabel.numberOfLines = 0
        statusLabel.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(statusLabel)

        NSLayoutConstraint.activate([
            spinner.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            spinner.centerYAnchor.constraint(equalTo: view.centerYAnchor),

            statusLabel.topAnchor.constraint(equalTo: spinner.bottomAnchor, constant: 16),
            statusLabel.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 24),
            statusLabel.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -24),

            errorLabel.topAnchor.constraint(equalTo: statusLabel.bottomAnchor, constant: 12),
            errorLabel.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 24),
            errorLabel.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -24),

            backButton.topAnchor.constraint(equalTo: errorLabel.bottomAnchor, constant: 16),
            backButton.centerXAnchor.constraint(equalTo: view.centerXAnchor),
        ])
        spinner.startAnimating()
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        guard !ran else { return }
        ran = true
        run()
    }

    @objc private func goBack() {
        dismiss(animated: false, completion: nil)
    }

    override func viewDidDisappear(_ animated: Bool) {
        super.viewDidDisappear(animated)
        stageTimer?.invalidate()
        stageTimer = nil
    }

    // DirtyJIT rule: never a bare spinner. The stage line plus a live
    // elapsed counter turns "does nothing" into "stuck at step X".
    private func setStage(_ s: String) {
        DispatchQueue.main.async {
            self.stageTimer?.invalidate()
            let t0 = Date()
            self.statusLabel.text = s
            self.stageTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) {
                [weak self] _ in
                guard let self = self else { return }
                let el = Int(Date().timeIntervalSince(t0))
                self.statusLabel.text = "\(s) (\(el)s)"
            }
        }
    }

    private func fail(_ message: String) {
        let detail = String(cString: whetstone_detail())
        UserDefaults.standard.set("failed", forKey: "ws_phase")
        UserDefaults.standard.synchronize()
        DispatchQueue.main.async {
            self.stageTimer?.invalidate()
            self.stageTimer = nil
            self.spinner.stopAnimating()
            self.errorLabel.text = message + detail
            self.errorLabel.isHidden = false
            self.backButton.isHidden = false
        }
    }

    private func run() {
        DispatchQueue.global(qos: .userInitiated).async {
            let fm = FileManager.default

            let attempts = UserDefaults.standard.integer(forKey: "ws_attempts") + 1
            let diedHere = UserDefaults.standard.string(forKey: "ws_phase") == "exploit"
            UserDefaults.standard.set(attempts, forKey: "ws_attempts")
            UserDefaults.standard.set("exploit", forKey: "ws_phase")
            // Forced: a panic seconds from now must not lose this.
            UserDefaults.standard.synchronize()

            if diedHere {
                // The last run died mid-exploit, so its trigon cache may be
                // half-written — trusting it would steer the next trigon
                // attempt into the same panic. Drop it.
                let home = ProcessInfo.processInfo.environment["HOME"] ?? NSHomeDirectory()
                try? fm.removeItem(atPath: home + "/Library/Caches/whetstone_trigon.cache")
            }

            // The only job: Amethyst-style self-jailbreak (exploit + root +
            // sandbox escape + platformize), so this process can patch
            // targets directly. A panic here returns via relaunch, not via
            // fail(). Rotate the flavor per attempt so a path that panics
            // deterministically is not retried blindly: auto, hemlock-only,
            // trigon-only, then repeat.
            let which: Int32
            let whichName: String
            switch (attempts - 1) % 3 {
            case 1: which = WHETSTONE_EXPLOIT_HEMLOCK; whichName = "hemlock"
            case 2: which = WHETSTONE_EXPLOIT_TRIGON; whichName = "trigon"
            default: which = WHETSTONE_EXPLOIT_AUTO; whichName = "auto"
            }
            self.setStage("Exploiting (\(whichName), attempt \(attempts))…")
            let rc = whetstone_run_exploit(which)
            guard rc == 0 else {
                self.fail(String(cString: whetstone_error_string(rc)))
                return
            }

            UserDefaults.standard.set("done", forKey: "ws_phase")
            UserDefaults.standard.set(0, forKey: "ws_attempts")
            UserDefaults.standard.synchronize()
            DispatchQueue.main.async {
                (UIApplication.shared.delegate as? AppDelegate)?.showApps()
            }
        }
    }
}
