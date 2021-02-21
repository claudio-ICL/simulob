#include "user.h"
#include "gui.h"
#include "orderbook.h"
#include <chrono>
#include <iostream>
#include <stdio.h>
#include <thread>
#define sleep_10 std::this_thread::sleep_for(std::chrono::milliseconds(10))
User::User(std::shared_ptr<OrderBook> book) { _book = book.get(); }
User::~User() {
  // printf("User Destructor\n");
}
void User::set_ordersubmission(OrderSubmission *os) { _ordersubmission = os; }
void User::stop_ordersubmission() {
  if (_ordersubmission)
    _ordersubmission->stop();
}
void User::set_userwindow(std::shared_ptr<UserWindow> uw) {
  // write("User::set_userwindow\n");
  if (uw) {
    _userwindow = uw.get();
    set_subwindow(uw);
  }
}
void User::send_order(int price, int size, OrderDirection d) {
  _book->add(std::make_shared<Order>(price, size, d));
  ++_submissions;
}
void User::send_order(OrderCommand cmd) {
  _book->add(std::move(cmd));
  ++_submissions;
}
void User::simulate() {
  _submissions = 0;
  if (_userwindow) {
    while (_go) {
      std::unique_lock<std::mutex> lock(_mtx);
      _cond.wait(lock);
      if (_go) {
        send_order(_userwindow->process_response());
      }
    }
  }
}
