# Dashboard Visualization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enhance the existing Qt director dashboard with date-range filtering, trend/bar visualizations, and CSV export without changing menus or permissions.

**Architecture:** Reuse the existing `dashboard:summary` request and response. The server adds chart arrays to the summary payload; the client renders them with lightweight `QWidget + QPainter` charts to avoid adding Qt Charts as a new dependency.

**Tech Stack:** C++17, Qt Widgets, Qt SQL, JSON Lines API, CMake source tests.

---

### Task 1: Source Test

**Files:**
- Modify: `tests/CMakeLists.txt`
- Create: `tests/dashboard_visualization_source_tests.cpp`

- [ ] Add a source test that requires date range payload fields, `dailyVisits`, `departmentVisits`, `doctorRanking`, chart widgets, `QDateEdit`, and CSV export logic.
- [ ] Register the test with CTest and set the working directory to the project root.
- [ ] Run `cmake --build build-codex --target dashboard_visualization_source_tests` and confirm the test fails before implementation.

### Task 2: Server Summary Data

**Files:**
- Modify: `server/src/modules/DashboardService.cpp`

- [ ] Parse optional `startDate` and `endDate` from the summary request payload.
- [ ] Add helper queries for daily outpatient visits, department visits, and doctor ranking over the selected date range.
- [ ] Add the arrays to both database and demo summary responses.
- [ ] Keep existing numeric cards and stock warnings intact.

### Task 3: Client Dashboard UI

**Files:**
- Modify: `client/include/client/pages/Pages.h`
- Modify: `client/src/pages/DashboardPage.cpp`

- [ ] Add date range controls, query button, and CSV export button.
- [ ] Add a reusable painted chart widget for line and bar modes.
- [ ] Render `dailyVisits`, `departmentVisits`, and `doctorRanking`.
- [ ] Store the latest rows and export them to CSV.
- [ ] Keep the existing tables and metric cards.

### Task 4: Verification

**Commands:**
- `cmake --build build-codex --target hospital_server hospital_client dashboard_visualization_source_tests`
- `ctest --test-dir build-codex -R "dashboard_visualization_source_tests|examination_workflow_source_tests|structured_medical_record_source_tests|doctor_cascade_filter_source_tests|refund_workflow_source_tests|permission_admin_source_tests|patient_identity_display_source_tests|auth_router_tests" --output-on-failure`

- [ ] If `hospital_server.exe` is locked, stop only the running `hospital_server` process and rerun the build.
- [ ] Report exactly which files changed and the verification output.
