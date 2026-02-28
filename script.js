const investmentFields = {
  stocks: 'Individual Stocks',
  mutualFunds: 'Individual Mutual Funds',
  gold: 'Gold',
  silver: 'Silver',
  crypto: 'Crypto',
  foreignUs: 'Foreign US Investment'
};

const liabilityFields = {
  carLoan: 'Car Loan',
  homeLoan: 'Home Loan',
  otherEmis: 'Other EMIs'
};

const allFieldIds = [...Object.keys(investmentFields), ...Object.keys(liabilityFields)];
const storageKey = 'networth-dashboard-values';

const currency = new Intl.NumberFormat('en-IN', {
  style: 'currency',
  currency: 'INR',
  minimumFractionDigits: 2
});

const summaryElements = {
  totalInvestment: document.getElementById('totalInvestment'),
  totalLiabilities: document.getElementById('totalLiabilities'),
  totalNetWorth: document.getElementById('totalNetWorth')
};

const graphView = document.getElementById('graphView');
const graphType = document.getElementById('graphType');
const chartCtx = document.getElementById('networthChart');
const resetBtn = document.getElementById('resetBtn');
const appLink = document.getElementById('appLink');
const copyLinkBtn = document.getElementById('copyLinkBtn');
let chart;

const palette = ['#2f6ce5', '#51b39e', '#8c61ff', '#f2a23a', '#e85b77', '#3ca8da'];

function safeNumber(value) {
  const parsed = Number(value);
  return Number.isFinite(parsed) && parsed > 0 ? parsed : 0;
}

function totalsFrom(fields) {
  return Object.keys(fields).reduce((acc, key) => {
    const input = document.getElementById(key);
    return acc + safeNumber(input.value);
  }, 0);
}

function dataFrom(fields) {
  const labels = [];
  const values = [];

  Object.entries(fields).forEach(([key, label]) => {
    const amount = safeNumber(document.getElementById(key).value);
    labels.push(label);
    values.push(amount);
  });

  return { labels, values };
}

function buildChartData(totalInvestment, totalLiabilities, netWorth) {
  const selectedView = graphView.value;

  if (selectedView === 'liabilityBreakdown') {
    const liabilityData = dataFrom(liabilityFields);
    return {
      labels: liabilityData.labels,
      datasets: [{
        label: 'Liability Amount',
        data: liabilityData.values,
        backgroundColor: palette.slice(0, liabilityData.values.length)
      }]
    };
  }

  if (selectedView === 'networthComparison') {
    return {
      labels: ['Total Investment', 'Total Liabilities', 'Total Net Worth'],
      datasets: [{
        label: 'Amount',
        data: [totalInvestment, totalLiabilities, netWorth],
        backgroundColor: ['#2f6ce5', '#e85b77', '#51b39e'],
        borderColor: ['#2f6ce5', '#e85b77', '#51b39e'],
        fill: false,
        tension: 0.3
      }]
    };
  }

  const investmentData = dataFrom(investmentFields);
  return {
    labels: investmentData.labels,
    datasets: [{
      label: 'Investment Amount',
      data: investmentData.values,
      backgroundColor: palette,
      borderColor: palette,
      fill: false,
      tension: 0.3
    }]
  };
}

function saveValues() {
  const values = allFieldIds.reduce((acc, key) => {
    acc[key] = document.getElementById(key).value;
    return acc;
  }, {});
  localStorage.setItem(storageKey, JSON.stringify(values));
}

function loadValues() {
  const saved = localStorage.getItem(storageKey);
  if (!saved) return;

  const parsed = JSON.parse(saved);
  allFieldIds.forEach((id) => {
    if (parsed[id] !== undefined) {
      document.getElementById(id).value = parsed[id];
    }
  });
}

function resetValues() {
  allFieldIds.forEach((id) => {
    document.getElementById(id).value = '0';
  });
  localStorage.removeItem(storageKey);
  updateDashboard();
}

function updateDashboard() {
  const totalInvestment = totalsFrom(investmentFields);
  const totalLiabilities = totalsFrom(liabilityFields);
  const netWorth = totalInvestment - totalLiabilities;

  summaryElements.totalInvestment.textContent = currency.format(totalInvestment);
  summaryElements.totalLiabilities.textContent = currency.format(totalLiabilities);
  summaryElements.totalNetWorth.textContent = currency.format(netWorth);

  const nextType = graphType.value;
  const chartData = buildChartData(totalInvestment, totalLiabilities, netWorth);

  if (chart) {
    chart.destroy();
  }

  chart = new Chart(chartCtx, {
    type: nextType,
    data: chartData,
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: {
          position: 'bottom'
        },
        tooltip: {
          callbacks: {
            label(context) {
              return `${context.label}: ${currency.format(context.raw)}`;
            }
          }
        }
      },
      scales: ['bar', 'line'].includes(nextType)
        ? {
            y: {
              beginAtZero: true,
              ticks: {
                callback(value) {
                  return currency.format(value);
                }
              }
            }
          }
        : {}
    }
  });

  saveValues();
}

function initializeAppLink() {
  const link = `${window.location.origin}${window.location.pathname}`;
  appLink.href = link;
  appLink.textContent = link;

  copyLinkBtn.addEventListener('click', async () => {
    try {
      await navigator.clipboard.writeText(link);
      copyLinkBtn.textContent = 'Copied!';
    } catch (error) {
      copyLinkBtn.textContent = 'Copy failed';
    }

    setTimeout(() => {
      copyLinkBtn.textContent = 'Copy Link';
    }, 1200);
  });
}

allFieldIds.forEach((id) => {
  document.getElementById(id).addEventListener('input', updateDashboard);
});

graphView.addEventListener('change', updateDashboard);
graphType.addEventListener('change', updateDashboard);
resetBtn.addEventListener('click', resetValues);

loadValues();
initializeAppLink();
updateDashboard();
