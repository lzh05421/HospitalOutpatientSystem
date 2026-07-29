export const USE_MOCK_DATA = true;

const PROTOCOL_ENDPOINT = '/api/router';

export interface DashboardStats {
  today_registrations: number;
  current_waiting: number;
  revenue: number;
}

export interface TopDoctor {
  doctor: string;
  visits: number;
}

export interface InventoryWarning {
  code: string;
  name: string;
  stock: number;
  threshold: number;
  level: 'low' | 'critical';
}

interface ProtocolRequest {
  module: string;
  action: string;
  payload?: Record<string, unknown>;
}

interface ProtocolResponse<T> {
  success: boolean;
  message?: string;
  data: T;
}

interface DashboardPayload {
  stats: DashboardStats;
  topDoctors: TopDoctor[];
  inventoryWarnings: InventoryWarning[];
  updatedAt: string;
}

const mockInventoryWarnings: InventoryWarning[] = [
  { code: 'D003', name: '奥美拉唑肠溶胶囊', stock: 8, threshold: 15, level: 'low' },
  { code: 'D005', name: '蒙脱石散', stock: 5, threshold: 10, level: 'critical' },
  { code: 'D012', name: '头孢克洛胶囊', stock: 11, threshold: 20, level: 'low' },
];

function normalizeNumber(value: unknown): number {
  const numeric = Number(value);
  return Number.isFinite(numeric) ? numeric : 0;
}

async function postProtocol<T>(request: ProtocolRequest): Promise<T> {
  const response = await fetch(PROTOCOL_ENDPOINT, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(request),
  });

  if (!response.ok) {
    throw new Error(`Dashboard request failed: HTTP ${response.status}`);
  }

  const body = (await response.json()) as ProtocolResponse<T>;
  if (!body.success) {
    throw new Error(body.message || 'Dashboard request failed');
  }
  return body.data;
}

async function fetchDashboardStats(): Promise<DashboardStats> {
  if (USE_MOCK_DATA) {
    return {
      today_registrations: 128,
      current_waiting: 5,
      revenue: 5000,
    };
  }

  const data = await postProtocol<Record<string, unknown>>({
    module: 'dashboard',
    action: 'stats',
  });

  return {
    today_registrations: normalizeNumber(data.today_registrations),
    current_waiting: normalizeNumber(data.current_waiting),
    revenue: normalizeNumber(data.revenue),
  };
}

async function fetchTopDoctors(): Promise<TopDoctor[]> {
  if (USE_MOCK_DATA) {
    return [
      { doctor: '张明', visits: 42 },
      { doctor: '李华', visits: 37 },
      { doctor: '周宁', visits: 31 },
      { doctor: '陈晓', visits: 24 },
      { doctor: '孙洁', visits: 19 },
    ];
  }

  const data = await postProtocol<{ rows?: Array<Record<string, unknown>> }>({
    module: 'dashboard',
    action: 'topDoctors',
  });

  return (data.rows || []).map((row) => ({
    doctor: String(row.doctor ?? row['医生'] ?? '-'),
    visits: normalizeNumber(row.visits ?? row['接诊量']),
  }));
}

export async function fetchDashboardData(): Promise<DashboardPayload> {
  const [stats, topDoctors] = await Promise.all([
    fetchDashboardStats(),
    fetchTopDoctors(),
  ]);

  return {
    stats,
    topDoctors,
    inventoryWarnings: USE_MOCK_DATA ? mockInventoryWarnings : [],
    updatedAt: new Date().toISOString(),
  };
}
