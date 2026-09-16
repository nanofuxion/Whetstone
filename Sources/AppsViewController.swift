import Darwin
import Foundation
import UIKit

// Pick an app to debug, then patch it directly: this process is jailbroken,
// so the kernel writes happen in-process (DirtyJIT shape, Amethyst means).
final class AppsViewController: UIViewController, UITableViewDataSource, UITableViewDelegate {
    private let titleLabel = UILabel()
    private let statusLabel = UILabel()
    private let table = UITableView()
    private let enableButton = UIButton(type: .system)
    private let doneButton = UIButton(type: .system)

    private var apps: [(bid: String, name: String, pid: pid_t?)] = []
    private var selected: (bid: String, name: String)?
    private var busy = false

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .white

        titleLabel.text = "Pick an app to debug"
        titleLabel.font = .boldSystemFont(ofSize: 20)
        titleLabel.textAlignment = .center

        statusLabel.font = .systemFont(ofSize: 12)
        statusLabel.textColor = .darkGray
        statusLabel.textAlignment = .center
        statusLabel.numberOfLines = 0

        table.dataSource = self
        table.delegate = self
        table.register(UITableViewCell.self, forCellReuseIdentifier: "cell")

        enableButton.setTitle("Enable debugging", for: .normal)
        enableButton.titleLabel?.font = .boldSystemFont(ofSize: 17)
        enableButton.addTarget(self, action: #selector(enableTapped), for: .touchUpInside)

        doneButton.setTitle("Done", for: .normal)
        doneButton.titleLabel?.font = .systemFont(ofSize: 15)
        doneButton.addTarget(self, action: #selector(doneTapped), for: .touchUpInside)

        for v in [titleLabel, statusLabel, table, enableButton, doneButton] as [UIView] {
            v.translatesAutoresizingMaskIntoConstraints = false
            view.addSubview(v)
        }
        let g = view.safeAreaLayoutGuide
        NSLayoutConstraint.activate([
            titleLabel.topAnchor.constraint(equalTo: g.topAnchor, constant: 12),
            titleLabel.leadingAnchor.constraint(equalTo: g.leadingAnchor, constant: 16),
            titleLabel.trailingAnchor.constraint(equalTo: g.trailingAnchor, constant: -16),

            statusLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 2),
            statusLabel.leadingAnchor.constraint(equalTo: g.leadingAnchor, constant: 16),
            statusLabel.trailingAnchor.constraint(equalTo: g.trailingAnchor, constant: -16),

            table.topAnchor.constraint(equalTo: statusLabel.bottomAnchor, constant: 8),
            table.leadingAnchor.constraint(equalTo: g.leadingAnchor),
            table.trailingAnchor.constraint(equalTo: g.trailingAnchor),
            table.heightAnchor.constraint(equalTo: g.heightAnchor, multiplier: 0.55),

            enableButton.topAnchor.constraint(equalTo: table.bottomAnchor, constant: 12),
            enableButton.centerXAnchor.constraint(equalTo: g.centerXAnchor),

            doneButton.topAnchor.constraint(equalTo: enableButton.bottomAnchor, constant: 8),
            doneButton.centerXAnchor.constraint(equalTo: g.centerXAnchor),
        ])

        NotificationCenter.default.addObserver(
            self, selector: #selector(targetNotified(_:)),
            name: NSNotification.Name("WhetstoneTarget"), object: nil)

        load()
    }

    // MARK: - list

    private func load() {
        var list = AppList.load().filter { $0.bundleID != Bundle.main.bundleIdentifier }
        list.sort {
            let r0 = AppList.pid(bundleID: $0.bundleID) != nil
            let r1 = AppList.pid(bundleID: $1.bundleID) != nil
            if r0 != r1 { return r0 && !r1 }
            return $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending
        }
        setApps(list.map { ($0.bundleID, $0.name, AppList.pid(bundleID: $0.bundleID)) }, note: nil)
    }

    private func setApps(_ apps: [(bid: String, name: String, pid: pid_t?)], note: String?) {
        self.apps = apps
        if let n = note { statusLabel.text = n }
        if let bid = (UIApplication.shared.delegate as? AppDelegate)?.pendingBundleID {
            (UIApplication.shared.delegate as? AppDelegate)?.pendingBundleID = nil
            select(bid: bid)
        } else if let sel = selected {
            select(bid: sel.bid)
        }
        updateButton()
        table.reloadData()
    }

    private func select(bid: String) {
        if let hit = apps.first(where: { $0.bid == bid }) {
            selected = (hit.bid, hit.name)
        } else {
            selected = nil
        }
        updateButton()
        table.reloadData()
    }

    private func updateButton() {
        if let sel = selected {
            enableButton.setTitle("Enable debugging on \(sel.name)", for: .normal)
        } else {
            enableButton.setTitle("Enable debugging", for: .normal)
        }
    }

    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        return apps.isEmpty ? 1 : apps.count
    }

    func tableView(_ tableView: UITableView,
                   cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueReusableCell(withIdentifier: "cell", for: indexPath)
        guard !apps.isEmpty else {
            cell.textLabel?.text = "Nothing found — open the target app, then come back."
            cell.accessoryType = .none
            return cell
        }
        let app = apps[indexPath.row]
        if let pid = app.pid {
            cell.textLabel?.text = "\(app.name) · running (\(pid))"
        } else {
            cell.textLabel?.text = app.name
        }
        cell.detailTextLabel?.text = app.bid
        cell.accessoryType = (app.bid == selected?.bid) ? .checkmark : .none
        return cell
    }

    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard !apps.isEmpty else { return }
        selected = (apps[indexPath.row].bid, apps[indexPath.row].name)
        updateButton()
        tableView.reloadData()
    }

    // MARK: - enable

    @objc private func targetNotified(_ note: Notification) {
        if let bid = note.object as? String, !bid.isEmpty { select(bid: bid) }
    }

    @objc private func enableTapped() {
        guard !busy else { return }
        guard let sel = selected else {
            statusLabel.text = "Tap an app in the list first."
            return
        }
        busy = true
        setBusy(true)
        statusLabel.text = "Starting \(sel.name)…"
        DispatchQueue.global(qos: .userInitiated).async {
            // Nothing needs to be running already: find the target, and if
            // it is not up, launch it through frontboard and catch its pid
            // (dasd first, exec-name scan as fallback).
            var pid = AppList.pid(bundleID: sel.bid)
            if pid == nil {
                DispatchQueue.main.async { self.open(bundleID: sel.bid) }
                let deadline = Date().addingTimeInterval(10)
                while Date() < deadline {
                    if let p = AppList.pid(bundleID: sel.bid) { pid = p; break }
                    Thread.sleep(forTimeInterval: 0.3)
                }
            }
            // The dasd pid can be stale (app quit, pid recycled): a dead
            // pid must read as failure, never as a patch attempt.
            guard let target = pid, kill(target, 0) == 0 else {
                DispatchQueue.main.async {
                    self.busy = false
                    self.setBusy(false)
                    self.statusLabel.text = "Couldn't start \(sel.name)."
                    self.load()
                }
                return
            }
            DispatchQueue.main.async { self.statusLabel.text = "Enabling…" }
            Whetstone.shared.enableJIT(pid: target) { [weak self] rc in
                guard let self = self else { return }
                self.busy = false
                self.setBusy(false)
                if rc == 0 {
                    self.statusLabel.text = "Debugging on for \(sel.name)."
                    self.open(bundleID: sel.bid)
                } else {
                    self.statusLabel.text = String(cString: whetstone_error_string(rc))
                }
            }
        }
    }

    @objc private func doneTapped() {
        (UIApplication.shared.delegate as? AppDelegate)?.showStart()
    }

    private func open(bundleID: String) {
        guard let wsClass = NSClassFromString("LSApplicationWorkspace") as? NSObject.Type,
              let ws = wsClass.perform(NSSelectorFromString("defaultWorkspace"))?
                  .takeUnretainedValue() as? NSObject else { return }
        _ = ws.perform(NSSelectorFromString("openApplicationWithBundleID:"), with: bundleID)
    }

    private func setBusy(_ b: Bool) {
        enableButton.isEnabled = !b
        doneButton.isEnabled = !b
    }
}
