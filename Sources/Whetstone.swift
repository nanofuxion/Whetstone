import Foundation
import UIKit

// Swift-side wrapper around the C exploit driver. All kernel work runs off
// the main thread; results come back on it.
final class Whetstone {
    static let shared = Whetstone()

    var log: ((String) -> Void)?

    func enableJIT(pid: pid_t, done: @escaping (Int32) -> Void) {
        DispatchQueue.global(qos: .userInitiated).async {
            var before: UInt32 = 0
            _ = whetstone_target_csflags(pid, &before)
            let rc = whetstone_enable_jit(pid)
            var after: UInt32 = 0
            _ = whetstone_target_csflags(pid, &after)
            DispatchQueue.main.async {
                if rc == 0 {
                    self.emit(String(format: "JIT on for pid %d (csflags %08x -> %08x).",
                                     pid, before, after))
                    self.emit("Open the target app now; it can run JIT until it quits or you reboot.")
                } else {
                    self.emit("JIT failed: \(String(cString: whetstone_error_string(rc)))")
                }
                done(rc)
            }
        }
    }

    private func emit(_ line: String) {
        DispatchQueue.main.async { self.log?(line) }
    }
}
