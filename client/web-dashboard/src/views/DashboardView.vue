<template>
  <section class="dashboard-shell">
    <header class="dashboard-header">
      <div>
        <p class="section-kicker">Hospital Command Center</p>
        <h1>院长驾驶舱</h1>
      </div>
      <div class="refresh-state" :class="{ stale: hasError }">
        <span class="status-dot" />
        <span>{{ statusText }}</span>
      </div>
    </header>

    <div class="metric-grid" aria-label="关键指标">
      <article v-for="metric in metrics" :key="metric.label" class="metric-card">
        <p>{{ metric.label }}</p>
        <strong>{{ metric.value }}</strong>
        <span>{{ metric.caption }}</span>
      </article>
    </div>

    <div class="content-grid">
      <section class="panel">
        <div class="panel-title">
          <h2>今日接诊 Top 5</h2>
          <span>按完成接诊量排序</span>
        </div>
        <table class="doctor-table">
          <thead>
            <tr>
              <th>排名</th>
              <th>医生</th>
              <th>接诊量</th>
              <th>负载</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(doctor, index) in topDoctors" :key="doctor.doctor">
              <td>{{ index + 1 }}</td>
              <td>{{ doctor.doctor || '-' }}</td>
              <td>{{ formatPlain(doctor.visits) }}</td>
              <td>
                <div class="load-track">
                  <span :style="{ width: loadWidth(doctor.visits) }" />
                </div>
              </td>
            </tr>
            <tr v-if="topDoctors.length === 0">
              <td colspan="4" class="empty-cell">暂无接诊数据</td>
            </tr>
          </tbody>
        </table>
      </section>

      <section class="panel inventory-panel">
        <div class="panel-title">
          <h2>库存预警</h2>
          <span>低于安全阈值药品</span>
        </div>
        <div class="warning-list">
          <article
            v-for="item in inventoryWarnings"
            :key="item.code"
            class="warning-item"
            :class="item.level"
          >
            <div>
              <strong>{{ item.name }}</strong>
              <span>{{ item.code }}</span>
            </div>
            <p>{{ item.stock }} / {{ item.threshold }}</p>
          </article>
          <p v-if="inventoryWarnings.length === 0" class="empty-note">暂无库存预警</p>
        </div>
      </section>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from 'vue';
import {
  fetchDashboardData,
  type DashboardStats,
  type InventoryWarning,
  type TopDoctor,
} from '../api/dashboardApi';

const stats = ref<DashboardStats>({
  today_registrations: 0,
  current_waiting: 0,
  revenue: 0,
});
const topDoctors = ref<TopDoctor[]>([]);
const inventoryWarnings = ref<InventoryWarning[]>([]);
const updatedAt = ref('');
const loading = ref(false);
const hasError = ref(false);
let timer: number | undefined;

function formatPlain(value: number): string {
  return value > 0 ? String(value) : '0';
}

function formatCurrency(value: number): string {
  return value > 0
    ? value.toLocaleString('zh-CN', { style: 'currency', currency: 'CNY' })
    : '¥0.00';
}

function loadWidth(value: number): string {
  const max = Math.max(...topDoctors.value.map((doctor) => doctor.visits), 1);
  return `${Math.max(8, Math.round((value / max) * 100))}%`;
}

async function refreshDashboard(): Promise<void> {
  loading.value = true;
  try {
    const data = await fetchDashboardData();
    stats.value = data.stats;
    topDoctors.value = data.topDoctors;
    inventoryWarnings.value = data.inventoryWarnings;
    updatedAt.value = data.updatedAt;
    hasError.value = false;
  } catch (error) {
    hasError.value = true;
    console.error(error);
  } finally {
    loading.value = false;
  }
}

const metrics = computed(() => [
  {
    label: '今日挂号量',
    value: formatPlain(stats.value.today_registrations),
    caption: '门诊总入口',
  },
  {
    label: '当前候诊',
    value: formatPlain(stats.value.current_waiting),
    caption: '等待叫号患者',
  },
  {
    label: '今日收入',
    value: formatCurrency(stats.value.revenue),
    caption: '支付流水汇总',
  },
  {
    label: '最高接诊',
    value: topDoctors.value[0]?.visits ? String(topDoctors.value[0].visits) : '-',
    caption: topDoctors.value[0]?.doctor || '暂无医生数据',
  },
]);

const statusText = computed(() => {
  if (loading.value) return '正在刷新';
  if (hasError.value) return '数据连接异常';
  if (!updatedAt.value) return '等待数据';
  return `已更新 ${new Date(updatedAt.value).toLocaleTimeString('zh-CN', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  })}`;
});

onMounted(() => {
  refreshDashboard();
  timer = window.setInterval(refreshDashboard, 10_000);
});

onUnmounted(() => {
  if (timer !== undefined) {
    window.clearInterval(timer);
  }
});
</script>

<style scoped>
.dashboard-shell {
  min-height: 100%;
  padding: 28px;
  color: #18212f;
  background:
    linear-gradient(135deg, rgba(16, 105, 107, 0.08), transparent 34%),
    linear-gradient(180deg, #f7faf9 0%, #eef3f1 100%);
  font-family: "Aptos", "Microsoft YaHei UI", "Noto Sans CJK SC", sans-serif;
}

.dashboard-header {
  display: flex;
  align-items: flex-end;
  justify-content: space-between;
  gap: 20px;
  margin-bottom: 22px;
}

.section-kicker {
  margin: 0 0 6px;
  color: #52706d;
  font-size: 12px;
  font-weight: 700;
  letter-spacing: 0;
  text-transform: uppercase;
}

h1,
h2,
p {
  margin: 0;
}

h1 {
  font-size: 30px;
  letter-spacing: 0;
}

.refresh-state {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  min-height: 34px;
  padding: 0 12px;
  border: 1px solid #bed8d3;
  border-radius: 8px;
  background: #ffffff;
  color: #245653;
  font-size: 13px;
}

.refresh-state.stale {
  border-color: #e5b4a8;
  color: #8f3829;
}

.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: #188f7b;
}

.refresh-state.stale .status-dot {
  background: #c94b35;
}

.metric-grid {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 14px;
  margin-bottom: 16px;
}

.metric-card,
.panel {
  border: 1px solid #d8e4e0;
  border-radius: 8px;
  background: rgba(255, 255, 255, 0.92);
  box-shadow: 0 10px 30px rgba(35, 55, 64, 0.08);
}

.metric-card {
  min-height: 112px;
  padding: 18px;
}

.metric-card p {
  color: #5b6c70;
  font-size: 13px;
  font-weight: 700;
}

.metric-card strong {
  display: block;
  margin-top: 12px;
  color: #0f3738;
  font-size: 28px;
  line-height: 1.1;
}

.metric-card span {
  display: block;
  margin-top: 10px;
  color: #738184;
  font-size: 12px;
}

.content-grid {
  display: grid;
  grid-template-columns: minmax(0, 1.35fr) minmax(320px, 0.65fr);
  gap: 16px;
}

.panel {
  min-height: 360px;
  padding: 18px;
}

.panel-title {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 12px;
  margin-bottom: 16px;
}

.panel-title h2 {
  font-size: 18px;
}

.panel-title span {
  color: #758487;
  font-size: 12px;
}

.doctor-table {
  width: 100%;
  border-collapse: collapse;
  table-layout: fixed;
}

.doctor-table th,
.doctor-table td {
  height: 46px;
  padding: 0 10px;
  border-bottom: 1px solid #e6eeeb;
  text-align: left;
  font-size: 14px;
}

.doctor-table th {
  color: #5d6e72;
  font-size: 12px;
  font-weight: 800;
}

.load-track {
  width: 100%;
  height: 8px;
  overflow: hidden;
  border-radius: 999px;
  background: #edf2ef;
}

.load-track span {
  display: block;
  height: 100%;
  border-radius: inherit;
  background: linear-gradient(90deg, #177f73, #c8a24d);
}

.warning-list {
  display: grid;
  gap: 10px;
}

.warning-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  min-height: 62px;
  padding: 12px;
  border: 1px solid #e5d5b2;
  border-radius: 8px;
  background: #fffaf0;
}

.warning-item.critical {
  border-color: #efb4a7;
  background: #fff4f1;
}

.warning-item strong {
  display: block;
  font-size: 14px;
}

.warning-item span {
  display: block;
  margin-top: 4px;
  color: #778286;
  font-size: 12px;
}

.warning-item p {
  color: #7b4c0f;
  font-weight: 800;
}

.warning-item.critical p {
  color: #a33624;
}

.empty-cell,
.empty-note {
  color: #7b8789;
  text-align: center;
}

@media (max-width: 980px) {
  .metric-grid,
  .content-grid {
    grid-template-columns: 1fr 1fr;
  }

  .inventory-panel {
    grid-column: 1 / -1;
  }
}

@media (max-width: 680px) {
  .dashboard-shell {
    padding: 18px;
  }

  .dashboard-header,
  .panel-title {
    align-items: flex-start;
    flex-direction: column;
  }

  .metric-grid,
  .content-grid {
    grid-template-columns: 1fr;
  }
}
</style>
