# Pharmacy Prescription Workflow Design

## Goal

Complete the pharmacy prescription workflow for outpatient prescriptions: review approval, review rejection with reason, dispense confirmation with stock deduction, and reverse drug return before billing refund.

## Scope

This first optimization point covers the prescription and pharmacy side only. Billing refund will be guarded where needed so paid bills with dispensed prescriptions cannot be refunded before pharmacy return, but the larger refund approval workflow remains part of the later refund optimization point.

## Business Rules

- A new prescription starts as `CREATED` and is displayed as `待审核`.
- A pharmacist can approve a `CREATED` prescription. It becomes `REVIEWED` and is displayed as `待发药`.
- A pharmacist can reject a `CREATED` prescription only with a non-empty reason. It becomes `REJECTED` and is displayed as `已驳回`.
- A rejected prescription is final. Doctors must create a new prescription instead of editing the rejected one.
- A pharmacist can dispense only a `REVIEWED` prescription whose related bill is `PAID`.
- Dispensing deducts every prescription item from inventory inside one transaction, writes stock records, and updates the prescription to `DISPENSED`.
- A pharmacist can return drugs only for a `DISPENSED` prescription. Return adds stock back, writes stock records, and updates the prescription to `RETURNED`.
- A dispensed prescription that has not been returned blocks billing refund.
- Repeated dispensing, repeated return, dispensing rejected prescriptions, and refunding before return are rejected with user-facing messages.

## Data Model

The existing `prescriptions` table gains audit fields:

- `reviewer_id`
- `review_time`
- `reject_reason`
- `dispense_user_id`
- `dispense_time`
- `return_user_id`
- `return_time`
- `return_reason`

The existing `stock_records.change_type` values are extended by convention:

- `OUT` for dispensing
- `RETURN` for pharmacy return

The existing `operation_logs` table records review, rejection, dispensing, return, and blocked refund decisions.

## Client Behavior

The Qt prescription page keeps the existing action bar and adds:

- Review dialog with `通过` and `驳回` choices.
- Required rejection reason when rejecting.
- Dispense confirmation dialog showing selected prescription patient, ID card, registration number, drug detail, amount, and current status.
- Return button for dispensed prescriptions with required return reason.

The prescription list includes rejection and pharmacy audit columns so doctors and pharmacy staff can see why a prescription is no longer actionable.

## Server Behavior

`PrescriptionService` supports these actions:

- `review`: approve `CREATED` prescriptions.
- `reject`: reject `CREATED` prescriptions with a reason.
- `dispense`: transactionally deduct stock and mark `DISPENSED`.
- `return`: transactionally add stock back and mark `RETURNED`.

`BillingService::refundBill` checks related prescriptions for the bill registration. If any prescription is `DISPENSED`, refund is blocked until pharmacy return changes it to `RETURNED`.

Demo mode mirrors these flows in `DemoRepository` so the project works without MySQL.

## Verification

Implementation must include failing-first tests for:

- Rejection requires a reason.
- Rejected prescriptions cannot be dispensed.
- Return is available only after dispensing.
- Returning adds stock back and prevents duplicate return.
- Billing refund is blocked while a prescription is dispensed and allowed after pharmacy return.
- Schema supports the new audit columns and `RETURNED`/`REJECTED` statuses.
