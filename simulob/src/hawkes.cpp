#include "hawkes.h"
#include "orderbook.h"
HawkesProcess::HawkesProcess(int dimension) : _numevents(dimension) {
  // write("HawkesProcess Constructor\n");
}
HawkesProcess::~HawkesProcess() {
  // write("HawkesProcess Destructor\n");
  _cond.notify_all();
}
void HawkesProcess::baserate(vector<double> rate) { _baserate = rate; }
void HawkesProcess::impactcoef(vector<vector<double>> coef) {
  _impactcoef = coef;
}
void HawkesProcess::decaycoef(vector<vector<double>> coef) {
  _decaycoef = coef;
}
void HawkesProcess::set_timeorigin(double t0) {
  _timeorigin = t0;
  _labelledtimes = vector<vector<double>>{{t0}, {t0}};
}
void HawkesProcess::add(double t, int e) {
  std::lock_guard<std::mutex> lock(_mtx);
  _labelledtimes[e].push_back(t);
  latest_event.arrival_time = t;
  latest_event.type = e;
  _cond.notify_one();
}
Event HawkesProcess::wait_for_event() {
  std::unique_lock<std::mutex> lock(_mtx);
  _cond.wait(lock);
  return latest_event;
}
vector<double> HawkesProcess::compute_intensities(double t) {
  vector<double> res(_numevents, 0.0);
  double ESSE[_numevents][_numevents];
  for (int e = 0; e < _numevents; ++e) {
    for (int e1 = 0; e1 < _numevents; ++e1) {
      ESSE[e1][e] = 0.0;
    }
  }
  std::lock_guard<std::mutex> lock(_mtx);
  for (int e = 0; e < _numevents; ++e) {
    for (int e1 = 0; e1 < _numevents; ++e1) {
      for (double s : _labelledtimes[e1]) {
        if (t <= s)
          break;
        double eval_time = 1.0 + t - s;
        double power_comp = pow(eval_time, -_decaycoef[e1][e]);
        ESSE[e1][e] += power_comp;
      }
      res[e] += _impactcoef[e1][e] * ESSE[e1][e];
    }
    res[e] += _baserate[e];
  }
  return res;
}
double HawkesProcess::total_intensity(vector<double> &intensities) {
  return std::accumulate(intensities.begin(), intensities.end(), 0.0);
}
void HawkesProcess::simulate() {
  // write("HawkesProcess::simulate()\n");
  // sleep10;
  double time = _timeorigin;
  vector<double> intensities = _baserate;
  double intensity_overall = total_intensity(intensities);
  std::random_device r;
  std::default_random_engine generator(r());
  std::uniform_real_distribution<double> uniform_distr(0, 1);
  while (this->go()) {
    double U = uniform_distr(generator);
    double random_exponential = -log(1.0 - U) / intensity_overall;
    long ms_sleep = static_cast<long>(random_exponential);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms_sleep));
    time += random_exponential;
    intensities = compute_intensities(time);
    double intensity_total = total_intensity(intensities);
    double random_uniform = intensity_overall * uniform_distr(generator);
    if (random_uniform < intensity_total) {
      std::discrete_distribution<int> distribution(intensities.begin(),
                                                   intensities.end());
      int event = distribution(generator);
      add(time, event);
      for (int e = 0; e < _numevents; ++e)
        intensities[e] += _impactcoef[event][e];
      intensity_total += std::accumulate(_impactcoef[event].begin(),
                                         _impactcoef[event].end(), 0.0);
    }
    intensity_overall = intensity_total;
  }
}
