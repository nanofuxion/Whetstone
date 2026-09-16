import Foundation
import UIKit

// An installed app we might enable JIT on.
struct WhetstoneApp {
    let bundleID: String
    let name: String
    let execName: String
    let bundleURL: URL
    let iconURL: URL?
}

// Listing apps needs filesystem access outside our sandbox, which only works
// after the exploit's sandbox escape. Before that every call returns [].
final class AppList {
    static let bundleDir = URL(fileURLWithPath: "/var/containers/Bundle/Application",
                               isDirectory: true)
    static let dasdPlist = "/var/mobile/Library/Preferences/com.apple.dasd.dock.persistence.plist"

    static func load() -> [WhetstoneApp] {
        let fm = FileManager.default
        guard let homes = try? fm.contentsOfDirectory(
            at: bundleDir, includingPropertiesForKeys: nil) else { return [] }
        var out: [WhetstoneApp] = []
        for home in homes {
            guard let kids = try? fm.contentsOfDirectory(
                at: home, includingPropertiesForKeys: nil) else { continue }
            guard let dotApp = kids.first(where: { $0.pathExtension == "app" }) else { continue }
            let infoURL = dotApp.appendingPathComponent("Info.plist")
            guard let info = NSDictionary(contentsOf: infoURL) as? [String: Any] else { continue }
            guard let bid = info["CFBundleIdentifier"] as? String else { continue }
            // Skip ourselves.
            if bid == Bundle.main.bundleIdentifier { continue }
            var name = (info["CFBundleDisplayName"] as? String)
                ?? (info["CFBundleName"] as? String) ?? bid
            if name.isEmpty { name = bid }
            // Executable name for the kernel-side lookup (p_comm). Falls
            // back to the .app dirname when Info.plist lacks the key.
            var exec = info["CFBundleExecutable"] as? String ?? ""
            if exec.isEmpty {
                exec = dotApp.deletingPathExtension().lastPathComponent
            }
            out.append(WhetstoneApp(bundleID: bid, name: name, execName: exec,
                                    bundleURL: dotApp, iconURL: iconURL(dotApp, info)))
        }
        execCache = Dictionary(uniqueKeysWithValues: out.map { ($0.bundleID, $0.execName) })
        return out.sorted { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
    }

    private static var execCache: [String: String] = [:]

    // Best-effort icon: first CFBundleIconFiles entry, else any png in bundle.
    private static func iconURL(_ dotApp: URL, _ info: [String: Any]) -> URL? {
        if let icons = info["CFBundleIcons"] as? [String: Any],
           let primary = icons["CFBundlePrimaryIcon"] as? [String: Any],
           let files = primary["CFBundleIconFiles"] as? [String],
           let first = files.last {
            for scale in ["@3x.png", "@2x.png", ".png"] {
                let u = dotApp.appendingPathComponent(first + scale)
                if FileManager.default.fileExists(atPath: u.path) { return u }
            }
        }
        return nil
    }

    // pid of a running app. dasd dock persistence plist first (exact
    // bid → pid when SpringBoard recorded it), then a live process scan
    // by executable name — dasd misses apps (fresh installs, previously
    // crash-on-launch probes) and the picker must not. Needs the sandbox
    // escape to read either source.
    static func pid(bundleID: String) -> pid_t? {
        if let p = pidFromDasd(bundleID: bundleID) { return p }
        guard let exec = execCache[bundleID], !exec.isEmpty else { return nil }
        let pid: pid_t = exec.withCString { whetstone_pid_for_exec($0) }
        return pid > 0 ? pid : nil
    }

    private static func pidFromDasd(bundleID: String) -> pid_t? {
        guard let raw = FileManager.default.contents(atPath: dasdPlist),
              let plist = try? PropertyListSerialization.propertyList(
                from: raw, options: [], format: nil) as? [String: Any],
              let ids = plist["applicationProcessIdentifiers"] as? [String: Any] else {
            return nil
        }
        if let n = ids[bundleID] as? Int { return pid_t(n) }
        if let n = ids[bundleID] as? Int64 { return pid_t(n) }
        return nil
    }
}
