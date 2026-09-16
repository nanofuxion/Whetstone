import Foundation
import UIKit

// Screen 1: one button, nothing else.
final class StartViewController: UIViewController {
    private let button = UIButton(type: .system)

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .white
        button.titleLabel?.font = .systemFont(ofSize: 22)
        button.addTarget(self, action: #selector(tapped), for: .touchUpInside)
        button.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(button)
        NSLayoutConstraint.activate([
            button.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            button.centerYAnchor.constraint(equalTo: view.centerYAnchor),
        ])
        refreshTitle()
    }

    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        refreshTitle()
    }

    private func refreshTitle() {
        if UserDefaults.standard.string(forKey: "ws_phase") == "exploit" {
            let n = UserDefaults.standard.integer(forKey: "ws_attempts")
            button.setTitle("Tap to retry — attempt \(n)", for: .normal)
        } else {
            button.setTitle("Tap to start", for: .normal)
        }
    }

    @objc private func tapped() {
        if !UserDefaults.standard.bool(forKey: "ws_preflight_shown") {
            UserDefaults.standard.set(true, forKey: "ws_preflight_shown")
            UserDefaults.standard.synchronize()
            let a = UIAlertController(
                title: "Before you start",
                message: "Reboot fresh and close other apps — the exploit needs free memory. " +
                    "If the iPad restarts, relaunch and tap again.",
                preferredStyle: .alert)
            a.addAction(UIAlertAction(title: "Cancel", style: .cancel, handler: nil))
            a.addAction(UIAlertAction(title: "Start", style: .default,
                                      handler: { [weak self] _ in self?.go() }))
            present(a, animated: true, completion: nil)
            return
        }
        go()
    }

    private func go() {
        present(LoadingViewController(), animated: false, completion: nil)
    }
}
