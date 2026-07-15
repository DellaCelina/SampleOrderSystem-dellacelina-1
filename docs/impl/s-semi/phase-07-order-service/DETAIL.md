# Phase 7: OrderService: submit/list/approve/reject/release

**Depends on:** Phase 5 (repositories), Phase 6 (production-service)
**Touches:** `SampleOrderSystem/Services/OrderService.h`, `SampleOrderSystem/Services/OrderService.cpp`, `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`

## Summary

Implement OrderService::SubmitOrder (validates sample exists and quantity>0, issues next order number, creates Reserved order), ListPendingApprovals (Reserved-only filter), Approve (calls ProductionService::SettleDueEntries first, then computes unclaimed stock = max(0, currentStock - sum of other Producing/Confirmed order quantities for that sample), decides Confirmed vs enqueue-and-Producing), Reject (Reserved->Rejected), and Release (Confirmed->Released with stock decrement, rejecting any other starting status). Covers the 50-then-100-then-100 unclaimed-stock acceptance scenario and the settle-then-decide ordering as unit tests against phase-6's ProductionService and phase-5's repositories. Add Services/OrderService.h/.cpp to SampleOrderSystemTests.vcxproj.

**Namespace/test-framework correction:** everything is in the **global namespace** (no
`Services::`/`Models::`/`Core::`/`Repositories::` wrappers), matching phase-1/2/4's real committed
code. `IClock::Now()` returns `std::chrono::system_clock::time_point` directly. Tests are
GoogleTest, not Catch2 — see phase-1's DETAIL.md superseding note.

## Detail

## Scope

Implement `SampleOrderSystem/Services/OrderService.h/.cpp`: the order-lifecycle service (`SubmitOrder`, `ListPendingApprovals`, `Approve`, `Reject`, `Release`), and wire the two new files plus their tests into `SampleOrderSystemTests.vcxproj` (per ARCHITECTURE.md's "Build/test wiring" section: add `<ClCompile>`/`<ClInclude>` items pointing at these files by relative path — no new project, no library).

This phase assumes the following already exist from phase-5 (models/repositories) and phase-6 (ProductionService), based on ARCHITECTURE.md's Components section. If the actual phase-5/6 signatures differ in naming, adapt call sites accordingly, but do not let that change OrderService's own public surface below, since a later controller phase depends on it:

```cpp
// Models/Order.h (phase-5)
enum class OrderStatus { Reserved, Confirmed, Producing, Released, Rejected };
struct Order {
    std::string orderNumber;   // "ORD-####"
    std::string sampleId;
    std::string customerName;
    int quantity;
    OrderStatus status;
};

// Models/Sample.h (phase-5)
struct Sample {
    std::string sampleId;
    std::string name;
    int averageProductionTimeMinutes;
    double yield;
    int currentStock;
};

// Repositories/OrderRepository.h (phase-5)
class OrderRepository {
public:
    std::string NextOrderNumber();                 // "ORD-0001", "ORD-0002", ... derived from max existing suffix at load
    void Add(const Order& order);           // append-only creation
    std::optional<Order> FindByOrderNumber(const std::string& orderNumber) const;
    std::vector<Order> FindAll() const;
    std::vector<Order> FindBySampleId(const std::string& sampleId) const;
    void UpdateStatus(const std::string& orderNumber, OrderStatus newStatus); // persists
};

// Repositories/SampleRepository.h (phase-5)
class SampleRepository {
public:
    std::optional<Sample> FindById(const std::string& sampleId) const;
    void AdjustStock(const std::string& sampleId, int delta); // currentStock += delta; persists
};

// Services/ProductionService.h (phase-6)
class ProductionService {
public:
    static int ComputeShortfall(int orderQuantity, int unclaimedStock); // max(orderQuantity - unclaimedStock, ...) — only called when unclaimedStock < orderQuantity, so no clamping needed here
    static int ComputeActualQuantity(int shortfall, double yield);      // ceil(shortfall / yield)
    ProductionQueueEntry Enqueue(const std::string& orderNumber, const std::string& sampleId,
                 int shortfallQuantity, IClock& clock);
        // per phase-6's real contract: takes only the already-computed shortfallQuantity (NOT
        // actualProducedQuantity — Enqueue looks up the sample and computes actualQty/duration
        // itself internally via the pure functions above), computes expectedCompletionAt from the
        // current queue tail (or clock.Now() if empty) + averageProductionTimeMinutes*actualProducedQuantity,
        // persists the ProductionQueueEntry to data/production_queue.json, and returns the created
        // entry by value. OrderService does NOT call ComputeActualQuantity itself before calling
        // Enqueue — that computation lives inside Enqueue, per phase-6's DETAIL.md.
    void SettleDueEntries(IClock& clock);
        // sweeps due entries, increments sample stock, flips matching order Producing->Confirmed, persists
};

// Core/IClock.h (phase-1/2, already exists)
class IClock { public: virtual std::chrono::system_clock::time_point Now() const = 0; virtual ~IClock() = default; };
```

If `OrderRepository::FindByOrderNumber`/`UpdateStatus` or `SampleRepository::AdjustStock` don't exist under these exact names in the real phase-5 output, use whatever equivalent phase-5 actually exposes (e.g. a generic `Update(Order)` taking a full record) — the important contract this phase relies on is: order lookup by order number, an order status mutation that persists, and a sample stock delta mutation that persists.

## Error/result shape

No exceptions are used for expected business-rule rejections (unknown sample, bad quantity, wrong status, unknown order number) — these are normal control flow, not bugs. Introduce a small result type in `OrderService.h`:

```cpp

enum class OrderServiceErrorCode {
    SampleNotFound,
    InvalidQuantity,
    OrderNotFound,
    InvalidStatusForApproval,
    InvalidStatusForRejection,
    InvalidStatusForRelease,
};

struct OrderServiceError {
    OrderServiceErrorCode code;
    std::string message; // human-readable, for eventual console display
};

template <typename T>
class OrderServiceResult {
public:
    static OrderServiceResult Success(T value);
    static OrderServiceResult Failure(OrderServiceErrorCode code, std::string message);
    bool Ok() const;
    explicit operator bool() const { return Ok(); }
    const T& Value() const;         // precondition: Ok()
    const OrderServiceError& Error() const; // precondition: !Ok()
private:
    bool ok_;
    std::optional<T> value_;
    std::optional<OrderServiceError> error_;
};

```

`ListPendingApprovals` has no failure mode, so it just returns `std::vector<Order>` directly (not wrapped).

## Public interface (`OrderService.h`)

```cpp

class OrderService {
public:
    OrderService(OrderRepository& orderRepository,
                 SampleRepository& sampleRepository,
                 ProductionService& productionService);

    OrderServiceResult<Order> SubmitOrder(const std::string& sampleId,
                                                   const std::string& customerName,
                                                   int quantity);

    std::vector<Order> ListPendingApprovals() const;

    OrderServiceResult<Order> Approve(const std::string& orderNumber, IClock& clock);

    OrderServiceResult<Order> Reject(const std::string& orderNumber);

    OrderServiceResult<Order> Release(const std::string& orderNumber);

private:
    int ComputeUnclaimedStock(const std::string& sampleId) const;

    OrderRepository& orderRepository_;
    SampleRepository& sampleRepository_;
    ProductionService& productionService_;
};

```

Note `ListPendingApprovals` and `SubmitOrder` deliberately take **no** `IClock&` — per ARCHITECTURE.md's Data Flow section, the lazy-settlement sweep before the pending-list *query* is the caller's (controller's) responsibility, not something `OrderService::ListPendingApprovals` does internally. Only `Approve` settles, because `Approve` itself is the thing reading+mutating order/stock/queue state that the requirement's lazy-settlement rule targets (Key Design Decision #3). Do not add a clock parameter to `ListPendingApprovals`/`SubmitOrder`/`Reject`/`Release` — `Reject`/`Release` don't touch production-queue timing at all, and adding an unused clock parameter to them would misrepresent that.

## Behavior

**`SubmitOrder(sampleId, customerName, quantity)`**
1. If `quantity <= 0` → `Failure(InvalidQuantity, ...)`, no repository writes at all (check this before the sample lookup or after — order doesn't matter for behavior, but do not create a partial order either way).
2. `sampleRepository_.FindById(sampleId)` — if not found → `Failure(SampleNotFound, ...)`, no order created.
3. Otherwise: `orderNumber = orderRepository_.NextOrderNumber()`, construct `Order{orderNumber, sampleId, customerName, quantity, OrderStatus::Reserved}`, `orderRepository_.Add(order)`, return `Success(order)`.
4. `customerName` is stored as-is (no validation specified in REQUIREMENT.md — empty string is technically accepted; do not invent a not-in-scope validation rule here).

**`ListPendingApprovals()`**
- Return every order with `status == Reserved`, in whatever order `OrderRepository::FindAll()` returns them (insertion/file order) — do not re-sort. Excludes all other statuses including `Rejected`.

**`ComputeUnclaimedStock(sampleId)` (private helper)**
- `sample = sampleRepository_.FindById(sampleId)` (must exist by the time this is called, since `Approve` already validated the order's sample when the order was submitted)
- `claimed = sum of order.quantity for every order with order.sampleId == sampleId and status in {Producing, Confirmed}`
- return `max(0, sample.currentStock - claimed)`
- This is the full order quantity for Producing/Confirmed orders, **not** their shortfall — this is the load-bearing detail behind the 50-then-100-then-100 scenario (Key Design Decision #2). Do not sum shortfalls here.

**`Approve(orderNumber, clock)`**
1. `productionService_.SettleDueEntries(clock)` — unconditionally, first, before any of the following reads. This must run even if the order turns out not to exist or not to be Reserved (settlement is a side effect of "querying" state, independent of whether this particular order is approvable) — call it before the order lookup, not after.
2. `order = orderRepository_.FindByOrderNumber(orderNumber)` — not found → `Failure(OrderNotFound, ...)`.
3. `order.status != Reserved` → `Failure(InvalidStatusForApproval, ...)`, no mutation.
4. `unclaimed = ComputeUnclaimedStock(order.sampleId)` (reads sample/orders *after* settlement, so reflects post-sweep reality).
5. If `unclaimed >= order.quantity`: `orderRepository_.UpdateStatus(orderNumber, Confirmed)`; no stock change; no queue entry; return `Success` with the updated order.
6. Else: `shortfall = ProductionService::ComputeShortfall(order.quantity, unclaimed)` (equivalently `order.quantity - unclaimed`, since `unclaimed < order.quantity` here so no clamping needed at this call site — the clamp already happened inside `ComputeUnclaimedStock`); `productionService_.Enqueue(orderNumber, order.sampleId, shortfall, clock)` — pass only the shortfall; `Enqueue` computes `actualProducedQuantity` internally via `ComputeActualQuantity` and looks up the sample's yield itself, so `OrderService` must **not** call `ComputeActualQuantity` before this call (this computes `expectedCompletionAt` off the *current* queue tail — correct only because step 1 already swept anything due, so no stale/expired tail entry is used as an anchor); `orderRepository_.UpdateStatus(orderNumber, Producing)`; return `Success`.
7. Sample stock is **not** touched during `Approve` in either branch — stock only changes via `SettleDueEntries` (production completing) and via `Release` (shipment decrementing it).

**`Reject(orderNumber)`**
1. `order = orderRepository_.FindByOrderNumber(orderNumber)` — not found → `Failure(OrderNotFound, ...)`.
2. `order.status != Reserved` → `Failure(InvalidStatusForRejection, ...)`, no mutation.
3. Else `orderRepository_.UpdateStatus(orderNumber, Rejected)`, return `Success`.
- No settlement call here — Reject doesn't read stock/queue state, only order status, and REQUIREMENT.md's lazy-settlement trigger list doesn't include "changing an order's own status by direct user action" as a settlement trigger distinct from a query.

**`Release(orderNumber)`**
1. `order = orderRepository_.FindByOrderNumber(orderNumber)` — not found → `Failure(OrderNotFound, ...)`.
2. `order.status != Confirmed` → `Failure(InvalidStatusForRelease, ...)` for **every** other status (`Reserved`, `Producing`, `Released`, `Rejected` all rejected identically), no mutation.
3. Else: `sampleRepository_.AdjustStock(order.sampleId, -order.quantity)`; `orderRepository_.UpdateStatus(orderNumber, Released)`; return `Success`.
- No settlement call — Release's precondition is the order's own already-current status; it doesn't need production-queue state to decide anything.

## Unit tests to write (against phase-5 repositories + phase-6 ProductionService, using a `FakeClock` for the Approve/settlement-ordering tests)

`SubmitOrder`
- Valid sample + positive quantity → returns `Success`; order has `status == Reserved`, a well-formed `ORD-####` number, and is persisted (`orderRepository_.FindByOrderNumber` finds it afterward).
- Sequential submissions get strictly increasing order numbers (`ORD-0001`, `ORD-0002`, ...).
- Unknown `sampleId` → `Failure(SampleNotFound)`; repository order count unchanged after the call.
- `quantity == 0` → `Failure(InvalidQuantity)`; no order created.
- `quantity < 0` (e.g. -5) → `Failure(InvalidQuantity)`; no order created.

`ListPendingApprovals`
- Returns only `Reserved` orders when the repository has a mix of all five statuses; `Rejected` orders never appear.
- Returns an empty list when there are no `Reserved` orders.

`Approve` — sufficient stock
- Sample stock 100, order quantity 60, no other Producing/Confirmed orders on that sample → `Approve` returns `Success`, order status becomes `Confirmed`, sample stock is unchanged, no production-queue entry was created.

`Approve` — insufficient stock
- Sample stock 30, order quantity 50, no other claims → shortfall computed as 20 (not 50); order becomes `Producing`; exactly one queue entry exists for this order with `shortfallQuantity == 20` and `actualProducedQuantity == ceil(20 / yield)`.

`Approve` — 50-then-100-then-100 acceptance scenario (this is the load-bearing regression test for Key Design Decision #2)
- Sample stock starts at 50, yield 1.0 (or any fixed yield) for simplicity of arithmetic.
- Submit order1 (qty 100), approve it → unclaimed = 50, shortfall = 50, order1 → `Producing`, queue entry for order1 has `shortfallQuantity == 50`.
- Submit order2 (qty 100), approve it → unclaimed must be computed as `max(0, 50 - 100) = 0` (order1's *full* 100, not its 50 shortfall, is what's claimed) → shortfall = 100, order2 → `Producing`, queue entry for order2 has `shortfallQuantity == 100`, not 150.

`Approve` — settle-then-decide ordering (regression test for Key Design Decision #3 / the settle-before-Approve-reads rule)
- Set up: sample stock 50; order1 (qty 100) submitted and approved at fake-clock time `t0` → Producing, queue entry with some `expectedCompletionAt`.
- Advance the `FakeClock` past order1's `expectedCompletionAt`.
- Submit order2 (qty 100) for the same sample, then call `Approve(order2, clock)` at the advanced time.
- Assert: order1 is now `Confirmed` (settlement flipped it as a side effect of `Approve` on a *different* order), sample stock reflects order1's production having landed, and order2's unclaimed-stock computation used the post-settlement numbers — i.e. order2's shortfall reflects order1 no longer being counted as a live Producing/Confirmed claim in the way it would if settlement hadn't run (verify the concrete shortfall number matches manual calculation from the now-current stock, not the pre-settlement stock).
- A second variant: verify the *queue tail* used for `ComputeCompletionTime`/`Enqueue` on the new entry is not anchored to order1's already-past `expectedCompletionAt` (i.e., if order1's entry was the only one in the queue and settlement removed it, order2's new entry's `enqueuedAt` anchor is `clock.Now()`, not `order1.expectedCompletionAt`, since order1 is no longer in the queue at all after settlement).

`Approve` — status/lookup errors
- Approving an order that doesn't exist → `Failure(OrderNotFound)`.
- Approving an order that is already `Confirmed`/`Producing`/`Released`/`Rejected` (parametrize over all four) → `Failure(InvalidStatusForApproval)`, order status unchanged, no queue entry created, no stock change.

`Reject`
- `Reserved` order → `Success`, status becomes `Rejected`.
- Non-`Reserved` order (parametrize over `Confirmed`/`Producing`/`Released`/`Rejected`) → `Failure(InvalidStatusForRejection)`, status unchanged.
- Unknown order number → `Failure(OrderNotFound)`.
- After rejection, the order no longer appears in `ListPendingApprovals`.

`Release`
- `Confirmed` order, quantity 30, sample stock 80 → `Success`, order becomes `Released`, sample stock becomes 50.
- Non-`Confirmed` order (parametrize over `Reserved`/`Producing`/`Released`/`Rejected`) → `Failure(InvalidStatusForRelease)`, status and stock unchanged for every one of those four.
- Unknown order number → `Failure(OrderNotFound)`.

## Build wiring

Add to `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`:
- `<ClInclude Include="..\SampleOrderSystem\Services\OrderService.h" />`
- `<ClCompile Include="..\SampleOrderSystem\Services\OrderService.cpp" />`
- The new test file (e.g. `Tests\OrderServiceTests.cpp`) as a `<ClCompile>` item inside the test project itself.

Do not add these to `SampleOrderSystem.vcxproj` under a different item type or duplicate the `<ClCompile>` entries there beyond what phase-5/6 already established — `SampleOrderSystem.vcxproj` already compiles `Services/*.cpp` as part of the main exe; this phase only needs the *test* project's item list extended to also compile `OrderService.cpp` into `SampleOrderSystemTests.exe`, per ARCHITECTURE.md's "Build/test wiring" (shared-sources-compiled-twice, no library project).

## Dependency/touches notes for the phase graph

- Depends on phase-5 (Models/Repositories: `Order`, `Sample`, `OrderRepository`, `SampleRepository`) and phase-6 (`ProductionService`, and transitively `Core/IClock`) — both are genuine compile-time dependencies (constructor parameters and method calls), not incidental.
- Touches only `SampleOrderSystem/Services/OrderService.h`, `SampleOrderSystem/Services/OrderService.cpp`, and `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj` (item-list additions). Does not touch `ProductionService.*`, any `Repositories/*`, or `Models/*` — if this phase's implementer finds a genuine need to change one of those (e.g. a missing repository method), that's a signal the phase-5/6 interface assumption above was wrong and should be reconciled with whatever those phases actually shipped, rather than silently expanding this phase's file list.

