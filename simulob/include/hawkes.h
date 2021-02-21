#pragma once
#include "controller.h"
#include "orders.h"
#include <chrono>
#include <condition_variable>
#include <math.h>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
using std::vector;
struct Event {
  double arrival_time;
  int type;
};
class HawkesProcess : public Controller {
public:
  HawkesProcess(int);
  ~HawkesProcess();
  void baserate(vector<double>);
  void impactcoef(vector<vector<double>>);
  void decaycoef(vector<vector<double>>);
  void set_timeorigin(double);
  void add(double, int);
  Event wait_for_event();
  void simulate();
  vector<double> compute_intensities(double);
  double total_intensity(vector<double> &);
  Event latest_event;

private:
  int _numevents{2};
  vector<double> _baserate;
  vector<vector<double>> _impactcoef;
  vector<vector<double>> _decaycoef;
  double _timeorigin{0.0};
  vector<vector<double>> _labelledtimes;
  std::mutex _mtx;
  std::condition_variable _cond;
};
