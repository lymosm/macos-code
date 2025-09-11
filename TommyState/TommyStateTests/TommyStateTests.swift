import XCTest
@testable import TommyState

final class TommyStateTests: XCTestCase {
    func testMonitorSimulation() {
        let m = MonitorManager.shared
        let oldFan = m.fanRPM
        m.forceRefreshNow()
        // Wait a bit for async update
        let exp = expectation(description: "wait")
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.2) {
            exp.fulfill()
        }
        waitForExpectations(timeout: 1.0)
        XCTAssertNotNil(m.fanRPM, "simulation should set fan value")
    }
}
