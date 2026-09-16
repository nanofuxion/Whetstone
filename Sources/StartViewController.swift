import Foundation
import UIKit

// Screen 1: one button, nothing else — plus a one-line hint. There is no
// retry state: every tap automatically tries both exploits, and a panic
// just means reopening and tapping again.
final class StartViewController: UIViewController {
    private let button = UIButton(type: .system)
    private let hintLabel = UILabel()

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .white
        button.setTitle("Tap to start", for: .normal)
        button.titleLabel?.font = .systemFont(ofSize: 22)
        button.addTarget(self, action: #selector(tapped), for: .touchUpInside)
        button.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(button)

        hintLabel.text = "If the device resprings, reopen Whetstone and tap to start again."
        hintLabel.font = .systemFont(ofSize: 13)
        hintLabel.textColor = .gray
        hintLabel.textAlignment = .center
        hintLabel.numberOfLines = 0
        hintLabel.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(hintLabel)

        NSLayoutConstraint.activate([
            button.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            button.centerYAnchor.constraint(equalTo: view.centerYAnchor),

            hintLabel.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 32),
            hintLabel.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -32),
            hintLabel.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -24),
        ])
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
