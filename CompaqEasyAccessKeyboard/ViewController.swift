import Cocoa
import SystemExtensions

class ViewController: NSViewController, OSSystemExtensionRequestDelegate {

    let driverID = "nz.indy.CompaqEasyAccessKeyboardDriver"

    @IBOutlet var textView: NSTextView!
    
    @IBAction func activateExtension(sender: AnyObject) {
        updateTextView(str: "Starting extenson")
        let request = OSSystemExtensionRequest.activationRequest(forExtensionWithIdentifier: driverID, queue: DispatchQueue.main)
        request.delegate = self
        let extensionManager = OSSystemExtensionManager.shared
        extensionManager.submitRequest(request)
    }
    
    @IBAction func deactivateExtension(sender: AnyObject) {
        updateTextView(str: "Stopping extenson")

        let request = OSSystemExtensionRequest.deactivationRequest(forExtensionWithIdentifier: driverID, queue: DispatchQueue.main)
        request.delegate = self
        let extensionManager = OSSystemExtensionManager.shared
        extensionManager.submitRequest(request)
    }
    
    func updateTextView(str: NSString) {
        let oldString = self.textView.string
        let newString = String(format: "%@%@\n", oldString, str)
        self.textView.string = newString
    }
    
    func request(_ request: OSSystemExtensionRequest, didFinishWithResult result: OSSystemExtensionRequest.Result) {
        updateTextView(str: "Extension request successful")
    }
    
    func request(_ request: OSSystemExtensionRequest, didFailWithError error: Error) {
        updateTextView(str: "Extension request failed")
    }
    
    func requestNeedsUserApproval(_ request: OSSystemExtensionRequest) {
        updateTextView(str: "Extension request requires approval. Open the Settings app to allow.")
    }
    
    func request(_ request: OSSystemExtensionRequest,
                 actionForReplacingExtension existing: OSSystemExtensionProperties,
                 withExtension ext: OSSystemExtensionProperties) -> OSSystemExtensionRequest.ReplacementAction {
        updateTextView(str: "Extension requires replacement")

        return .replace
    }

}

