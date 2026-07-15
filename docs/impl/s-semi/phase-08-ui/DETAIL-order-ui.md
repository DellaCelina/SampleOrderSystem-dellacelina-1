# Phase 11: Order UI (OrderView + OrderController)

**Depends on:** Phase 7 (order-service)
**Touches:** `SampleOrderSystem/Views/OrderView.h`, `SampleOrderSystem/Views/OrderView.cpp`, `SampleOrderSystem/Controllers/OrderController.h`, `SampleOrderSystem/Controllers/OrderController.cpp`

## Summary

Implement the console rendering and controller logic for order submission, the pending-approvals list, approve/reject actions, and release, wiring console input/output to OrderService's corresponding methods. No file/JSON access in either View or Controller; all persistence flows through OrderService (phase-7). Independent of Sample UI (phase-10) and Monitoring/DataMonitor/DummyData UI (phase-12/13) — disjoint files, no shared dependency beyond the already-built services.

## Detail

## Assumed upstream contract (from phase-7, `SampleOrderSystem/Services/OrderService.h`)

This phase does not modify `OrderService`; it only calls it. Before starting, open the actual `OrderService.h` produced by phase-7 and reconcile it against the shape assumed below (derived from `docs/ARCHITECTURE.md`'s Data Flow section). If names differ, adapt call sites 1:1 — the tests in this phase are written against *behavior* (success/failure flag, resulting status, error message), not against exact identifier spelling, so a rename is a mechanical fix, not a redesign.

```cpp
namespace sos {

enum class OrderStatus { Reserved, Confirmed, Producing, Released, Rejected };
// Assume Models/Order.h already exposes: std::string ToString(OrderStatus); (from an earlier phase)

struct Order {
    std::string orderNumber;
    std::string sampleId;
    std::string customerName;
    int quantity;
    OrderStatus status;
};

struct SubmitOrderResult {
    bool success;
    std::string orderNumber;   // valid only when success
    std::string errorMessage;  // valid only when !success
};

struct ApproveResult {
    bool success;
    OrderStatus resultingStatus; // Confirmed or Producing, valid only when success
    std::string errorMessage;    // valid only when !success
};

struct ActionResult {           // used by Reject and Release
    bool success;
    std::string errorMessage;   // valid only when !success
};

class OrderService {
public:
    SubmitOrderResult SubmitOrder(const std::string& sampleId, const std::string& customerName, int quantity);
    std::vector<Order> ListPendingApprovals();          // Reserved-only; settles due production entries first
    ApproveResult Approve(const std::string& orderNumber); // settles first, then decides Confirmed/Producing
    ActionResult Reject(const std::string& orderNumber);
    ActionResult Release(const std::string& orderNumber);
};

} // namespace sos
```

Key point this phase relies on: `ListPendingApprovals()` and `Approve()` already perform lazy settlement internally (per architecture's "Lazy settlement" data-flow paragraph) — `OrderController` never calls `ProductionService::SettleDueEntries` itself and never touches `ProductionService`/repositories/`IClock` directly. `OrderController` only ever talks to `OrderService`.

## Files and their responsibilities

- `SampleOrderSystem/Views/OrderView.h` / `.cpp` — a class that only formats and writes to an injected `std::ostream&`. No reads, no file I/O, no business logic (no branching on business rules — only on which `Show*` method was called and the data passed to it).
- `SampleOrderSystem/Controllers/OrderController.h` / `.cpp` — reads from an injected `std::istream&`, calls `OrderService`, and calls `OrderView` methods with the results. No direct console (`std::cin`/`std::cout`) references, no file/JSON access, no direct dependency on `ProductionService`, repositories, or `IClock`.

Both constructors take their I/O streams by reference so production wiring (in the later `main.cpp`/`MainMenuController` phase) passes `std::cin`/`std::cout`, and this phase's own tests pass `std::istringstream`/`std::ostringstream`. This is the load-bearing design decision that makes Controller/View testable without a real console or without needing `OrderService` test doubles beyond what phase-7 already ships (a real `OrderService` wired to phase-7's own fake-clock/temp-directory test fixtures is reused here, not re-invented).

## `OrderView` — public interface

```cpp
class OrderView {
public:
    explicit OrderView(std::ostream& out);

    void ShowOrderMenu();               // prints the 4 order-management options (see below)
    void ShowInvalidMenuChoice();

    void ShowSubmitOrderResult(bool success, const std::string& orderNumber, const std::string& errorMessage);
    // success: renders the new order number and that it is RESERVED.
    // !success: renders errorMessage; orderNumber argument is ignored/blank in this case.

    void ShowPendingApprovals(const std::vector<sos::Order>& orders); // called only when orders is non-empty
    void ShowNoPendingApprovals();

    void ShowInvalidApprovalAction(); // action letter wasn't A/R — pure input-format issue, no service call was made

    void ShowApproveResult(bool success, const std::string& orderNumber, sos::OrderStatus resultingStatus, const std::string& errorMessage);
    void ShowRejectResult(bool success, const std::string& orderNumber, const std::string& errorMessage);
    void ShowReleaseResult(bool success, const std::string& orderNumber, const std::string& errorMessage);

private:
    std::ostream& out_;
};
```

Notes:
- `ShowPendingApprovals` is only ever invoked by the controller when the list is non-empty; the empty case is a separate `ShowNoPendingApprovals()` call so the View never has to branch on emptiness internally — keeps each method a straight-line formatter, easy to unit test with one assertion per case.
- Each row rendered by `ShowPendingApprovals` must include the order number, customer name, sample ID, and quantity (required by REQUIREMENT.md's pending-approvals acceptance criterion) — exact column layout/wording is not prescribed (per ARCHITECTURE.md Open Question #3, UI copy is language/format-agnostic), but it must be a stable, greppable one-row-per-order format so controller tests can assert "the rendered text contains this order's number" without over-fitting to a specific layout.
- There is intentionally no `ShowInvalidOrderNumber()`. An order-number token that doesn't correspond to a real `Reserved` order is *not* a controller-level format error — it's forwarded to `OrderService::Approve/Reject/Release`, which returns `success=false` with an `errorMessage`, rendered via the existing `ShowApproveResult`/`ShowRejectResult`/`ShowReleaseResult` failure branch. This avoids duplicating "does this order exist / is it in the right state" logic in the Controller when the Service already owns and reports it.
- No method returns a string for the Controller to further branch on formatting — View methods are terminal: they write directly to `out_` and return `void`.

## `OrderController` — public interface

```cpp
class OrderController {
public:
    OrderController(sos::OrderService& orderService, OrderView& view, std::istream& in);

    void Run(); // the only method the future MainMenuController phase needs to call

private:
    void HandleSubmitOrder();
    void HandleApproveReject();
    void HandleRelease();

    // returns false (and leaves out unchanged) if text is not all-digits or parses to <= 0
    static bool TryParsePositiveInt(const std::string& text, int& out);

    static std::string Trim(const std::string& text);
    bool ReadLine(std::string& out); // reads one line from in_, trims it; returns false on EOF/stream failure

    sos::OrderService& orderService_;
    OrderView& view_;
    std::istream& in_;
};
```

`Run()` is the sole contract this phase exposes to whatever later phase builds `MainMenuController`: it owns display and looping of the Order submenu itself (the caller does not need to know about `HandleSubmitOrder`/etc., and does not pass it any per-call input — `Run()` reads everything itself from the `in_` given at construction, and returns when the user backs out). This mirrors how `MainMenuController` is expected to dispatch to each feature area's controller (per ARCHITECTURE.md's Console MVC layer / `main.cpp` description) without needing to know that area's internal menu structure.

## Menu structure and exact flows

**Order submenu** (`Run()` loop, printed by `ShowOrderMenu()`):
```
1) Submit new order
2) Review pending approvals (approve/reject)
3) Release a confirmed order
4) Back
```
`Run()` reads one line via `ReadLine`; on EOF/stream failure it returns immediately (treated the same as "Back", so a test can end a scripted input stream without an explicit "4"). On `"1"` → `HandleSubmitOrder()`; `"2"` → `HandleApproveReject()`; `"3"` → `HandleRelease()`; `"4"` → return; anything else → `view_.ShowInvalidMenuChoice()` and loop again (does not exit, does not crash).

**`HandleSubmitOrder()`**: single-shot, no retry loop (chosen for determinism/testability — REQUIREMENT.md doesn't mandate reprompting, and Non-goals rule out any mid-`RESERVED` editing flow, so keeping submission itself simple and one-shot is consistent).
1. Read Sample ID line, read Customer name line, read Quantity line (three `ReadLine` calls, in that order).
2. If Sample ID or Customer name is blank after trim, or Quantity doesn't satisfy `TryParsePositiveInt`, call `view_.ShowSubmitOrderResult(false, "", <error>)` **without calling `OrderService::SubmitOrder` at all** and return. (Controller-local guard only for input *format*; "sample ID doesn't exist" is a business rule left to the Service — see below.)
3. Otherwise call `orderService_.SubmitOrder(sampleId, customerName, quantity)` and render its result verbatim via `ShowSubmitOrderResult` (this is what surfaces a "sample not found" business-rule rejection from phase-7, since the Controller performed no existence pre-check).

**`HandleApproveReject()`**: loops until the user backs out.
1. Call `orderService_.ListPendingApprovals()`.
2. If empty → `view_.ShowNoPendingApprovals()` and return immediately (no further prompt this call — do not block on an order-number read when there's nothing to act on).
3. If non-empty → `view_.ShowPendingApprovals(orders)`, then read the "order number to act on" line. Blank line or the literal token `"0"` → return (back to Order submenu). Any other non-blank token is taken as the order number as-is (no existence/format pre-check — forwarded straight to the Service).
4. Read the "Approve or Reject? (A/R)" line; take the trimmed first character, case-insensitive.
   - `'A'` → call `orderService_.Approve(orderNumber)`, render via `ShowApproveResult(result.success, orderNumber, result.resultingStatus, result.errorMessage)`.
   - `'R'` → call `orderService_.Reject(orderNumber)`, render via `ShowRejectResult(result.success, orderNumber, result.errorMessage)`.
   - anything else (including an empty line) → call `view_.ShowInvalidApprovalAction()` and make **no** service call (order stays untouched).
5. Loop back to step 1 (re-fetch the pending list — this is what re-settles/refreshes state before the next iteration, and is why no order-number/action pair is ever acted on twice against stale data).

**`HandleRelease()`**: single-shot, no loop.
1. Read the "Order number to release" line. Blank → return without calling the Service (treated as cancel, consistent with the blank-cancels convention used in `HandleApproveReject`).
2. Otherwise call `orderService_.Release(orderNumber)` and render via `ShowReleaseResult(result.success, orderNumber, result.errorMessage)` — success and failure (e.g. order not `Confirmed`, or unknown order number) both rendered, no pre-check duplicated in the Controller.

## Unit tests to write

### `OrderView` tests (construct with `std::ostringstream`, assert on `.str()`)
1. `ShowOrderMenu` output contains all four numbered options.
2. `ShowInvalidMenuChoice` produces some non-empty message (guards against a silent no-op).
3. `ShowSubmitOrderResult(true, "ORD-0007", "")` — output contains `"ORD-0007"` and indicates `RESERVED`/success; does not contain any error text.
4. `ShowSubmitOrderResult(false, "", "Sample not found")` — output contains the error message and does not contain a fabricated order number.
5. `ShowPendingApprovals` with 2+ orders — output contains each order's number, customer, sample ID, and quantity, one row per order (assert substrings for both rows, not just the first).
6. `ShowNoPendingApprovals` — non-empty, distinct message (not identical text to `ShowPendingApprovals`'s empty-implied case, since that method is never called with an empty vector).
7. `ShowInvalidApprovalAction` — non-empty, distinct message.
8. `ShowApproveResult(true, "ORD-0001", OrderStatus::Confirmed, "")` — output mentions `ORD-0001` and `CONFIRMED` (via `ToString(OrderStatus)`), not `PRODUCING`.
9. `ShowApproveResult(true, "ORD-0002", OrderStatus::Producing, "")` — output mentions `PRODUCING`.
10. `ShowApproveResult(false, "ORD-0003", /*ignored*/ OrderStatus::Confirmed, "not RESERVED")` — output contains the error message, not a fabricated resulting status.
11. `ShowRejectResult` success and failure cases (analogous to 8/10 but no status variant needed since Reject only ever results in `Rejected`).
12. `ShowReleaseResult` success and failure cases.

### `OrderController` tests
Use a real `sos::OrderService` wired against phase-7's test fixtures (fake `IClock`, and either the same temp-directory-backed repositories phase-7's own tests use, or in-memory repository doubles if phase-7 exposes them for testing — reuse whatever phase-7 already built rather than inventing a second seam). Seed known `Sample`/`Order` state per test via the repositories directly. Drive `OrderController` with `std::istringstream` scripted input and `std::ostringstream` captured output; assert both on rendered output substrings and on resulting state (re-querying the repository/service) so a test can't pass on the right message with the wrong side effect or vice versa.

1. **Menu dispatch, valid choice**: input `"1"` + valid submit fields + `"4"` → after `Run()` returns, a new `Reserved` order exists with an `ORD-####`-formatted number, and output contains that number.
2. **Menu dispatch, invalid choice then exit**: input `"9"` then `"4"` → `ShowInvalidMenuChoice` text appears, `Run()` returns cleanly (no crash, no infinite loop), no side effects.
3. **Menu dispatch, EOF-as-back**: input stream exhausted with no `"4"` at all → `Run()` returns without throwing.
4. **Submit, success path**: sample exists with sufficient unclaimed stock isn't actually required for `Reserved` creation (submission never checks stock — only sample existence and quantity), so assert a `Reserved` order is created for a valid known `sampleId`.
5. **Submit, blank sample ID**: input has an empty first line → `OrderService::SubmitOrder` is never called (assert order count unchanged before/after) and `ShowSubmitOrderResult(false, ...)` path is rendered.
6. **Submit, non-numeric quantity** (e.g. `"abc"`): same as above — no Service call, failure rendered.
7. **Submit, zero/negative quantity** (`"0"`, `"-5"`): same as above — no Service call, failure rendered.
8. **Submit, unknown sample ID**: Controller does *not* guard this — `OrderService::SubmitOrder` is called and returns `success=false`; assert the Service's own error message is what's rendered, and no order was created.
9. **Approve/Reject, empty pending list**: no `Reserved` orders exist → `ShowNoPendingApprovals` rendered; the flow returns without attempting to read an order-number line at all (achievable by giving the scripted input stream nothing further after the menu choice `"2"` and confirming no hang/exception, then feeding `"4"` afterward to exit `Run()`).
10. **Approve, sufficient unclaimed stock**: seed a sample with enough unclaimed stock and one `Reserved` order for it; input `order number` + `"A"` → order transitions to `Confirmed`; `ShowApproveResult(true, ..., Confirmed, "")` rendered.
11. **Approve, insufficient stock**: seed a sample with less unclaimed stock than requested; same flow → order transitions to `Producing`; `ShowApproveResult(true, ..., Producing, "")` rendered.
12. **Reject**: `Reserved` order + input order number + `"R"` → order transitions to `Rejected`; `ShowRejectResult(true, ...)` rendered; a subsequent `ListPendingApprovals()` no longer includes it.
13. **Approve/Reject, invalid action letter** (e.g. `"x"`): order stays `Reserved` (assert unchanged), `ShowInvalidApprovalAction` rendered, no Service call made — verify by checking the order's status is untouched afterward (cannot assert "Service not called" directly without a spy, so assert via state).
14. **Approve/Reject, exit token `"0"`/blank at the order-number prompt**: returns to the Order submenu without attempting any action/letter read.
15. **Approve/Reject, order number for a nonexistent/non-Reserved order**: Service's failure path is rendered via `ShowApproveResult`/`ShowRejectResult(false, ...)`, no crash, no state change.
16. **Approve/Reject, loop continues after one action**: script two order-number+action pairs in sequence (e.g. approve one order, then reject another) followed by `"0"` to exit — assert both orders end in their expected final status, proving the list is re-fetched and the loop doesn't exit after a single action.
17. **Release, success**: a `Confirmed` order + its order number as input → transitions to `Released`; `ShowReleaseResult(true, ...)` rendered.
18. **Release, wrong status** (order still `Reserved`, or already `Released`): failure rendered, no state change.
19. **Release, nonexistent order number**: failure rendered (Service's error message), no crash.
20. **Release, blank input**: no Service call made, no rendering beyond nothing/cancel (assert `Release` was never invoked, via unchanged state of every order).

## Explicit non-goals / boundaries for this phase
- No stock/monitoring numbers are rendered anywhere in this phase's Views — that's `MonitoringView`/`ProductionLineView` (a different, already-separately-scoped phase); `OrderView` shows only order-level fields (number, customer, sample ID, quantity, status).
- No modification of `RESERVED` orders (edit/cancel) is implemented anywhere here, per REQUIREMENT.md's Non-goals — the Order submenu intentionally has exactly the three action items above plus Back, nothing else.
- This phase performs no `ProductionService`/repository/`IClock` wiring — all of that is assumed already available via the injected `OrderService&` from phase-7. If phase-7's constructor requires additional collaborators to build an `OrderService` for tests, that wiring lives in this phase's test setup code only, not in `OrderController` itself.
- Exact console copy/strings are this phase's own decision (per ARCHITECTURE.md Open Question #3) — tests should assert on structurally meaningful substrings (order numbers, status names, presence/absence of an error) rather than pinning exact sentence wording, so future wording tweaks don't require rewriting this phase's whole test suite.
