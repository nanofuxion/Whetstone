import Foundation
import UIKit

@UIApplicationMain
final class AppDelegate: UIResponder, UIApplicationDelegate {

    var window: UIWindow?
    var pendingBundleID: String?

    func application(_ application: UIApplication,
                     didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        window = UIWindow(frame: UIScreen.main.bounds)
        showInitial()
        window?.makeKeyAndVisible()
        return true
    }

    // Every launch starts here: the exploit dies with the process, so
    // there is nothing to resume — same as any semi-untethered jailbreak.
    func showInitial() {
        window?.rootViewController = StartViewController()
    }

    func showApps() {
        window?.rootViewController = AppsViewController()
    }

    func showStart() {
        window?.rootViewController = StartViewController()
    }

    // whetstone://enable-jit?bundle-id=<bid> — a client app hands us its id.
    func application(_ app: UIApplication, open url: URL,
                     options: [UIApplication.OpenURLOptionsKey: Any] = [:]) -> Bool {
        guard url.scheme == "whetstone", url.host == "enable-jit" else { return false }
        let parts = URLComponents(url: url, resolvingAgainstBaseURL: false)
        let bid = parts?.queryItems?.first(where: { $0.name == "bundle-id" })?.value
        guard let id = bid, !id.isEmpty else { return false }
        if window?.rootViewController is AppsViewController {
            NotificationCenter.default.post(name: NSNotification.Name("WhetstoneTarget"), object: id)
        } else {
            pendingBundleID = id
        }
        return true
    }
}
