# Web Dashboard Adapter

This folder contains a Vue 3 implementation of the director dashboard.

## Files

- `src/api/dashboardApi.ts`
  - `fetchDashboardData()`
  - Builds backend protocol requests with `module/action`.
  - `USE_MOCK_DATA = true` lets the page render without the C++ server.

- `src/views/DashboardView.vue`
  - Four metric cards.
  - Top 5 doctors table.
  - Inventory warning panel with mock placeholder data.
  - Polls every 10 seconds and clears the timer on unmount.

## Backend Protocol Mapping

The C++ backend is not an HTTP server. It uses a module/action protocol.
A browser frontend needs an HTTP gateway, Electron preload bridge, or another host adapter that forwards:

```json
{ "module": "dashboard", "action": "stats" }
```

and:

```json
{ "module": "dashboard", "action": "topDoctors" }
```

`dashboardApi.ts` posts these protocol requests to `/api/router` when `USE_MOCK_DATA` is `false`.
