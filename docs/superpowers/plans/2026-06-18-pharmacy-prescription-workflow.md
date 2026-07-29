# Pharmacy Prescription Workflow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete pharmacy prescription review, rejection, dispensing, return, and refund blocking for dispensed prescriptions.

**Architecture:** Extend the existing Qt/C++ module services instead of introducing a new module. Keep the state machine in `PrescriptionService` and `DemoRepository`, use existing `stock_records` and `operation_logs`, and add minimal schema columns for pharmacy audit fields.

**Tech Stack:** C++17, Qt Widgets, Qt SQL, Qt Test, CMake, MySQL schema SQL.

---

## File Structure

- Modify `database/schema.sql`: add prescription audit columns and seed examples for rejected/returned statuses only where useful.
- Modify `server/src/modules/PrescriptionService.cpp`: add reject and return actions, review audit fields, dispense audit fields, operation logs, and list columns.
- Modify `server/src/modules/BillingService.cpp`: block refund when related prescriptions are `DISPENSED`.
- Modify `server/src/DemoRepository.cpp`: mirror reject, return, audit fields, inventory return, and refund block in demo mode.
- Modify `server/include/server/DemoRepository.h`: declare `rejectPrescription` and `returnPrescription`.
- Modify `client/src/pages/PrescriptionPage.cpp`: add reject path in review dialog, dispense detail confirmation, and return button/dialog.
- Modify `client/include/client/pages/Pages.h`: declare `returnPrescription`.
- Create `tests/pharmacy_workflow_source_tests.cpp`: source/schema tests that fail before implementation and verify required workflow hooks.
- Modify `tests/CMakeLists.txt`: register the new test.

## Task 1: Tests and Schema Contract

**Files:**
- Create: `tests/pharmacy_workflow_source_tests.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `database/schema.sql`

- [ ] **Step 1: Write the failing test**

Create a source/schema test that reads `PrescriptionService.cpp`, `BillingService.cpp`, `DemoRepository.cpp`, `PrescriptionPage.cpp`, and `database/schema.sql`. Assert the code contains `reject`, `return`, `REJECTED`, `RETURNED`, `reject_reason`, `return_reason`, `change_type, quantity`, `RETURN`, and a refund guard for `DISPENSED`.

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-windows --target pharmacy_workflow_source_tests` then `ctest --test-dir build-windows -R pharmacy_workflow_source_tests --output-on-failure`

Expected: fail because the new test executable or assertions are not satisfied before implementation.

- [ ] **Step 3: Add schema columns**

Add `reviewer_id`, `review_time`, `reject_reason`, `dispense_user_id`, `dispense_time`, `return_user_id`, `return_time`, and `return_reason` to `prescriptions`.

- [ ] **Step 4: Run test to verify partial progress**

Run the same test. Expected: still fail until service/client/demo hooks are implemented.

## Task 2: Server Workflow

**Files:**
- Modify: `server/src/modules/PrescriptionService.cpp`
- Modify: `server/src/modules/BillingService.cpp`

- [ ] **Step 1: Extend prescription review**

Update `reviewPrescriptionInDatabase` to only approve `CREATED`, set `REVIEWED`, bind `reviewer_id`, and set `review_time`.

- [ ] **Step 2: Add reject action**

Add `rejectPrescriptionInDatabase`. It must require `驳回原因`, update only `CREATED` prescriptions to `REJECTED`, save `reviewer_id`, `review_time`, and `reject_reason`, and write an `operation_logs` row.

- [ ] **Step 3: Harden dispense**

Keep the existing transaction, reject `REJECTED` and `RETURNED` explicitly, set `dispense_user_id` and `dispense_time`, and write an operation log after successful stock deduction.

- [ ] **Step 4: Add return action**

Add `returnPrescriptionInDatabase`. It must require `退药原因`, operate only on `DISPENSED`, lock prescription items, add stock back, insert `stock_records` with `change_type = 'RETURN'`, set `RETURNED`, `return_user_id`, `return_time`, and `return_reason`, and write an operation log.

- [ ] **Step 5: Block billing refund**

In `BillingService::refundBill`, inside a transaction, load the bill registration, reject refund if any related prescription has `status = 'DISPENSED'`, log the blocked decision, otherwise set bill `REFUNDED`.

## Task 3: Demo Repository Workflow

**Files:**
- Modify: `server/include/server/DemoRepository.h`
- Modify: `server/src/DemoRepository.cpp`

- [ ] **Step 1: Declare and implement reject**

Add `rejectPrescription`. Require `驳回原因`, only allow `待审核` or `CREATED`, set `已驳回`, `驳回原因`, `审核人`, and `审核时间`.

- [ ] **Step 2: Harden demo dispense**

Reject `已驳回` and `已退药`, keep paid-bill and inventory checks, set dispense audit fields, and append an operation log.

- [ ] **Step 3: Implement demo return**

Add `returnPrescription`. Require `退药原因`, only allow `已发药`, add inventory back, set `已退药`, `退药原因`, `退药人`, and `退药时间`.

- [ ] **Step 4: Block demo refund**

Before demo refund, find prescriptions for the bill registration and block if any is `已发药`.

## Task 4: Client Controls

**Files:**
- Modify: `client/include/client/pages/Pages.h`
- Modify: `client/src/pages/PrescriptionPage.cpp`

- [ ] **Step 1: Add return button**

Add `退药入库` visible to pharmacy roles and connect it to `returnPrescription`.

- [ ] **Step 2: Replace one-click review**

Make `reviewPrescription` open a dialog with approve/reject choice and a reason field. Send action `review` for approve and `reject` for reject.

- [ ] **Step 3: Improve dispense confirmation**

Show selected prescription details before sending `dispense`.

- [ ] **Step 4: Add return dialog**

Require return reason and send action `return`.

## Task 5: Verification

**Files:**
- All touched files

- [ ] **Step 1: Run targeted test**

Run: `cmake --build build-windows --target pharmacy_workflow_source_tests` and `ctest --test-dir build-windows -R pharmacy_workflow_source_tests --output-on-failure`

Expected: pass.

- [ ] **Step 2: Run broader tests**

Run: `ctest --test-dir build-windows --output-on-failure`

Expected: pass or report unrelated pre-existing failures with exact names and output.

- [ ] **Step 3: Manual source review**

Check that no pseudo-code or placeholder text was added, and that all new request actions are wired in server, demo, and client.
