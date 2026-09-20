import Foundation
import UIKit

@_silgen_name("csops")
private func csops(_ pid: pid_t, _ ops: UInt32,
                   _ useraddr: UnsafeMutableRawPointer?, _ usersize: Int) -> Int32

private let CS_OPS_STATUS: UInt32 = 0
private let CS_DEBUGGED: UInt32 = 0x1000_0000

// One-screen result display. Only kernel-safe checks run automatically
// (CS_DEBUGGED state + map-without-exec). Actually executing generated code
// happens solely on the exec button: on stock iOS the kernel answers with
// SIGKILL — uncatchable by design — so a marker file is fsynced first and a
// kill on relaunch reads as "JIT OFF (killed during <probe>)", which is the
// result. After Whetstone patches this app the same button reports PASS.
final class ProbeViewController: UIViewController {

    private let verdictLabel = UILabel()
    private let infoLabel = UILabel()
    private let rwxLabel = UILabel()
    private let rwrxLabel = UILabel()
    private let mapjitLabel = UILabel()
    private let execButton = UIButton(type: .system)
    private let rerunButton = UIButton(type: .system)
    private let whetstoneButton = UIButton(type: .system)

    private var execResults: [Int?] = [nil, nil, nil]

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .white

        verdictLabel.text = "…"
        verdictLabel.font = .boldSystemFont(ofSize: 30)
        verdictLabel.textAlignment = .center

        for l in [infoLabel, rwxLabel, rwrxLabel, mapjitLabel] as [UILabel] {
            l.font = .systemFont(ofSize: 14)
            l.numberOfLines = 0
        }

        execButton.setTitle("Run exec tests", for: .normal)
        execButton.titleLabel?.font = .boldSystemFont(ofSize: 17)
        execButton.addTarget(self, action: #selector(runExecTests), for: .touchUpInside)

        rerunButton.setTitle("Re-run safe tests", for: .normal)
        rerunButton.titleLabel?.font = .systemFont(ofSize: 15)
        rerunButton.addTarget(self, action: #selector(runSafeTests), for: .touchUpInside)

        whetstoneButton.setTitle("Open in Whetstone", for: .normal)
        whetstoneButton.titleLabel?.font = .systemFont(ofSize: 17)
        whetstoneButton.addTarget(self, action: #selector(openWhetstone), for: .touchUpInside)

        for v in [verdictLabel, infoLabel, rwxLabel, rwrxLabel,
                  mapjitLabel, execButton, rerunButton, whetstoneButton] as [UIView] {
            v.translatesAutoresizingMaskIntoConstraints = false
            view.addSubview(v)
        }
        let g = view.safeAreaLayoutGuide
        NSLayoutConstraint.activate([
            verdictLabel.topAnchor.constraint(equalTo: g.topAnchor, constant: 24),
            verdictLabel.leadingAnchor.constraint(equalTo: g.leadingAnchor, constant: 16),
            verdictLabel.trailingAnchor.constraint(equalTo: g.trailingAnchor, constant: -16),

            infoLabel.topAnchor.constraint(equalTo: verdictLabel.bottomAnchor, constant: 16),
            infoLabel.leadingAnchor.constraint(equalTo: g.leadingAnchor, constant: 16),
            infoLabel.trailingAnchor.constraint(equalTo: g.trailingAnchor, constant: -16),

            rwxLabel.topAnchor.constraint(equalTo: infoLabel.bottomAnchor, constant: 16),
            rwxLabel.leadingAnchor.constraint(equalTo: g.leadingAnchor, constant: 16),
            rwxLabel.trailingAnchor.constraint(equalTo: g.trailingAnchor, constant: -16),

            rwrxLabel.topAnchor.constraint(equalTo: rwxLabel.bottomAnchor, constant: 8),
            rwrxLabel.leadingAnchor.constraint(equalTo: g.leadingAnchor, constant: 16),
            rwrxLabel.trailingAnchor.constraint(equalTo: g.trailingAnchor, constant: -16),

            mapjitLabel.topAnchor.constraint(equalTo: rwrxLabel.bottomAnchor, constant: 8),
            mapjitLabel.leadingAnchor.constraint(equalTo: g.leadingAnchor, constant: 16),
            mapjitLabel.trailingAnchor.constraint(equalTo: g.trailingAnchor, constant: -16),

            execButton.topAnchor.constraint(equalTo: mapjitLabel.bottomAnchor, constant: 24),
            execButton.centerXAnchor.constraint(equalTo: g.centerXAnchor),

            rerunButton.topAnchor.constraint(equalTo: execButton.bottomAnchor, constant: 12),
            rerunButton.centerXAnchor.constraint(equalTo: g.centerXAnchor),

            whetstoneButton.topAnchor.constraint(equalTo: rerunButton.bottomAnchor, constant: 12),
            whetstoneButton.centerXAnchor.constraint(equalTo: g.centerXAnchor),
        ])

        NotificationCenter.default.addObserver(
            self, selector: #selector(runSafeTests),
            name: UIApplication.willEnterForegroundNotification, object: nil)
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        runSafeTests()
    }

    private var debugged: Bool {
        var flags: UInt32 = 0
        let ok = csops(getpid(), CS_OPS_STATUS, &flags,
                       MemoryLayout.size(ofValue: flags)) == 0
        return ok && (flags & CS_DEBUGGED) != 0
    }

    // MARK: - crash marker (a kernel kill is a result, not a mystery)

    private func markerURL() -> URL {
        FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("jitprobe_last.txt")
    }

    private func writeMarker(_ s: String) {
        try? s.write(to: markerURL(), atomically: false, encoding: .utf8)
        if let fh = try? FileHandle(forWritingTo: markerURL()) {
            fh.synchronizeFile()
            fh.closeFile()
        }
    }

    private func readMarker() -> String? {
        guard let s = try? String(contentsOf: markerURL(), encoding: .utf8),
              !s.isEmpty else { return nil }
        return s
    }

    private func clearMarker() {
        try? FileManager.default.removeItem(at: markerURL())
    }

    // MARK: - tests

    @objc private func runSafeTests() {
        let bid = Bundle.main.bundleIdentifier ?? "?"
        let pid = getpid()
        let dbg = debugged

        let killed = readMarker()
        clearMarker()

        let rwx = probe_rwx_noexec()
        let rwrx = probe_rwrx_noexec()
        let mj = probe_mapjit_noexec()
        let maxVA = Double(probe_max_va()) / 1_073_741_824.0

        infoLabel.text = "\(bid)\npid \(pid) · CS_DEBUGGED: \(dbg ? "yes" : "no")" +
            String(format: "\nmax VA reservation: %.1f GB", maxVA)
        updateRows(mapResults: [rwx, rwrx, mj])

        if let k = killed {
            verdictLabel.text = "JIT OFF"
            verdictLabel.textColor = UIColor(red: 0.75, green: 0.15, blue: 0.1, alpha: 1)
            infoLabel.text = (infoLabel.text ?? "") +
                "\nLast run was killed during \(k): kernel refused unsigned code."
        } else {
            updateVerdict()
        }
    }

    @objc private func runExecTests() {
        // Marker first, fsynced: if the kernel kills us mid-probe, the next
        // launch reports exactly where.
        let probes: [(String, () -> Int32)] = [
            ("mmap RWX", { Int32(probe_rwx()) }),
            ("mmap RW→RX", { Int32(probe_rw_then_rx()) }),
            ("mmap MAP_JIT", { Int32(probe_map_jit()) }),
        ]
        for (i, p) in probes.enumerated() {
            writeMarker(p.0)
            execResults[i] = Int(p.1())
            clearMarker()
        }
        // Re-read state too: patching flips CS_DEBUGGED and may move the
        // VA ceiling, so the exec run reports everything fresh.
        let bid = Bundle.main.bundleIdentifier ?? "?"
        let dbg = debugged
        let maxVA = Double(probe_max_va()) / 1_073_741_824.0
        infoLabel.text = "\(bid)\npid \(getpid()) · CS_DEBUGGED: \(dbg ? "yes" : "no")" +
            String(format: "\nmax VA reservation: %.1f GB", maxVA)
        updateRows(mapResults: nil)
        updateVerdict()
    }

    private func updateRows(mapResults: [Int32]?) {
        let maps: [Int32]
        if let m = mapResults {
            maps = m
        } else {
            maps = [probe_rwx_noexec(), probe_rwrx_noexec(), probe_mapjit_noexec()]
        }
        let names = ["mmap RWX", "mmap RW → mprotect RX", "mmap MAP_JIT"]
        let labels = [rwxLabel, rwrxLabel, mapjitLabel]
        for i in 0..<3 {
            let mapText = String(cString: probe_describe_map(maps[i]))
            if let e = execResults[i] {
                let execText = String(cString: probe_describe(Int32(e)))
                labels[i].text = "\(names[i]):\n  map: \(mapText)\n  exec: \(execText)"
            } else {
                labels[i].text = "\(names[i]):\n  map: \(mapText)\n  exec: not run"
            }
        }
    }

    private func updateVerdict() {
        let anyPass = execResults.contains(where: { $0 == 0 })
        if anyPass {
            verdictLabel.text = "JIT WORKS"
            verdictLabel.textColor = UIColor(red: 0.05, green: 0.55, blue: 0.2, alpha: 1)
        } else if execResults.allSatisfy({ $0 == nil }) {
            verdictLabel.text = "TAP RUN"
            verdictLabel.textColor = .darkGray
        } else {
            verdictLabel.text = "JIT OFF"
            verdictLabel.textColor = UIColor(red: 0.75, green: 0.15, blue: 0.1, alpha: 1)
        }
    }

    @objc private func openWhetstone() {
        let bid = Bundle.main.bundleIdentifier ?? ""
        guard let url = URL(string: "whetstone://enable-jit?bundle-id=\(bid)"),
              UIApplication.shared.canOpenURL(url) else { return }
        UIApplication.shared.open(url, options: [:], completionHandler: nil)
    }
}
