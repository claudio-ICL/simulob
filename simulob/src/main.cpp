#include "gui.h"
#include "orderbook.h"
#include "user.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <map>
#include <memory>
#include <ncurses.h>
#include <stdio.h>
#include <thread>
int main() {
  std::shared_ptr<OrderBook> ob =
      std::make_shared<OrderBook>(100, 100000, 100500);
  double decayrate = 0.0000001;
  std::shared_ptr<OrderSubmission> osub =
      std::make_shared<OrderSubmission>(ob, decayrate);
  osub->hawkes.baserate(std::vector<double>{0.1, 0.1});
  osub->hawkes.impactcoef(
      std::vector<std::vector<double>>{{0.75, 0.03}, {0.03, 0.5}});
  osub->hawkes.decaycoef(
      std::vector<std::vector<double>>{{2.40, 2.5}, {2.5, 2.5}});
  osub->hawkes.set_timeorigin(0.0);
  {
    int status_y = 1;
    int user_y = 7;
    int sparrow_y = 4;
    IntPanel interface =
        Interface::setup(status_y, sparrow_y, user_y, osub->user());
    Interface::setup(interface, ob, osub);
    Interface::init(interface);
    std::thread t(Interface::display, std::ref(interface));
    ob->launch_simulation();
    osub->launch_simulation();
    t.join();
  }
  osub->join();
  ob->join();
  Interface::wait_for_F1();
  Interface::close();
  return 0;
}
