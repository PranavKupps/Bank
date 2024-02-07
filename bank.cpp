// Project identifier: 292F24D17A4455C1B5133EDD8C7CEAA0C9570A98

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <vector>
#include <stdio.h>
#include <getopt.h>

using namespace std;

option redirect[] = {
  {  "help",       no_argument, nullptr,  'h'},
  { "verbose",     no_argument, nullptr,  'v'},
  { "file",  required_argument, nullptr,  'f'},
  { nullptr,                 0, nullptr, '\0'}
};

struct transaction {
  string user;
  string recepient;
  uint64_t place_time;
  uint64_t exec_date;
  uint32_t transaction_ID;
  uint32_t amount;
  uint16_t fee;
  bool shared;
};

struct User {
  vector<transaction*> incoming_transactions;
  vector<transaction*> outgoing_transactions;
  unordered_map<string, bool> validIPs;
  string user_ID;
  uint64_t reg_time;
  uint32_t pin;
  uint32_t balance;
  int num_incoming_transaction;
  int num_outgoing_transaction;
  bool active_session;
};

//EFFECTS: Returns true if rhs is higher priority than lhs
class Compare {
public:
  bool operator()(const transaction *rhs, const transaction *lhs) const {
    if(rhs->exec_date == lhs->exec_date) {
      return rhs->transaction_ID > lhs->transaction_ID; //put something else instead
    } else {
      return rhs->exec_date > lhs->exec_date;
    }
  };
};

class program {
public:
  priority_queue<transaction*, deque<transaction*>, Compare > pending_transactions;
  unordered_map<string, User*> user_masterlist; //hash map
  vector<transaction*> transaction_masterlist;
  bool verbose;
  bool commands_stage;

  uint64_t remove_timestamp_colons(string &timestamp_colon) {
    string new_num;
    for (uint32_t i = 0; i < timestamp_colon.size(); i++) { 
      if(timestamp_colon[i] != ':') {
        new_num = new_num + timestamp_colon[i];
      }
    } 
    return static_cast<uint64_t>(std::stoll(new_num)); //only keeps the first number of the 
  }

  void login(string &user_ID_input, uint32_t pin_input,  string &IP_address_input) {
    auto it = user_masterlist.find(user_ID_input);

    if(it == user_masterlist.end()) {
      if(verbose) {
        cout << "Failed to log in " << user_ID_input << ".\n";
      }
    } else {
      if(it->second->pin != pin_input) {
        if(verbose) {
          cout << "Failed to log in " << user_ID_input << ".\n";
        }
      } else {
        auto t = it->second->validIPs.find(IP_address_input);
        if(t == it->second->validIPs.end()) {
          it->second->validIPs.insert({IP_address_input, true});
          it->second->active_session = true;
        }
        if(verbose) {
          cout << "User " << user_ID_input << " logged in.\n";
        }
      }
    }
  }

  void logout(string &user_ID_input, string &IP_address_input) {
    auto it = user_masterlist.find(user_ID_input);

    if(it == user_masterlist.end()) {
      if(verbose) {
        cout << "Failed to log out " << user_ID_input << ".\n";
      }
    } else if(!(it->second->active_session)) {
      if(verbose) {
        cout << "Failed to log out " << user_ID_input << ".\n";
      }
    } else {
      auto ip = it->second->validIPs.find(IP_address_input);
      if(ip == it->second->validIPs.end()) {
        if(verbose) {
          cout << "Failed to log out " << user_ID_input << ".\n";
        }
      } else {
        it->second->validIPs.erase(ip);

        if(it->second->validIPs.empty()) {
          it->second->active_session = false;
        }

        if(verbose) {
          cout << "User " << user_ID_input <<" logged out.\n";
        }
      }
    }
  }

  uint16_t calculate_fee(const transaction *t) {
    uint16_t fee = static_cast<uint16_t>((t->amount) / 100);
    auto it = user_masterlist.find(t->user);

    if(fee < 10) {
      fee = 10;
    } else if (fee > 450) {
      fee = 450;
    }
    
    if((t->place_time - it->second->reg_time)/100 > (5 * 100 * 100 * 100 * 100)) {
      fee = static_cast<uint16_t>((fee * 3) / 4);
    }

    return fee;
  }
  
  bool check_transaction(const transaction &t, string &IP_adress_check) {
    auto u = user_masterlist.find(t.user);
    auto r = user_masterlist.find(t.recepient);
    
    if((t.exec_date - t.place_time) > (3 * 100 * 100 * 100)) { //check if exec_date is not more than 3 days from place_time
      if(verbose) {
        cout << "Select a time less than three days in the future.\n";
      }
      return false;
    } else if ((u == user_masterlist.end()) || (r == user_masterlist.end())) { //check user and reciepient exist
      if(verbose) {
        if(u == user_masterlist.end()) {
          cout << "Sender " << t.user << " does not exist.\n";
        } else {
          cout << "Recipient " << t.recepient << " does not exist.\n";
        }
      }
      return false;
    } else if(t.place_time > t.exec_date) {
      cerr << "You cannot have an execution date before the current timestamp.\n";
      return 1;
    } else if(( t.place_time < u->second->reg_time) || (t.place_time < r->second->reg_time)) { //check the place time is after reg times
      if(verbose) {
        cout << "At the time of execution, sender and/or recipient have not registered.\n";
      } 
      return false;
    } else if(!u->second->active_session) {
      if(verbose) {
        cout << "Sender " << t.user << " is not logged in.\n";
      }
      return false;
    } else {
      for(auto it: u->second->validIPs) {
        if(IP_adress_check == it.first) {
          return true;
        }
      }
      
      if(verbose) {
        cout << "Fraudulent transaction detected, aborting request.\n";
      }
      return false;
    }
  }

  void execute_transactions() {
    transaction *current_transaction = pending_transactions.top();
    pending_transactions.pop();

    uint32_t user_fee = current_transaction->fee / 2;
    if(current_transaction->fee % 2 != 0) {
      user_fee++;
    }
    uint32_t recipient_fee = current_transaction->fee - user_fee;

    auto u = user_masterlist.find(current_transaction->user);
    auto r = user_masterlist.find(current_transaction->recepient);

    if((!current_transaction->shared)) {
      if(u->second->balance < (current_transaction->fee + current_transaction->amount)) { //check for sufficient funds
        if(verbose) {
          cout << "Insufficient funds to process transaction " << current_transaction->transaction_ID << ".\n";
        }
        delete current_transaction;
      } else { //process transaction
        r->second->balance = r->second->balance + current_transaction->amount;
        r->second->incoming_transactions.push_back(current_transaction);
        r->second->num_incoming_transaction++;
        if(r->second->incoming_transactions.size() > 10) {
          r->second->incoming_transactions.erase(r->second->incoming_transactions.begin());
        }
        u->second->balance = u->second->balance - current_transaction->amount - current_transaction->fee;
        u->second->outgoing_transactions.push_back(current_transaction);
        u->second->num_outgoing_transaction++;
        if(u->second->outgoing_transactions.size() > 10) {
          u->second->outgoing_transactions.erase(u->second->outgoing_transactions.begin());
        }
        transaction_masterlist.push_back(current_transaction);
        if(verbose) {
          cout << "Transaction executed at " << current_transaction->exec_date << ": $" << current_transaction->amount
            << " from " << current_transaction->user << " to " << current_transaction->recepient << ".\n";
        }
      }
    } else {
      if((u->second->balance < (user_fee + current_transaction->amount)) || (r->second->balance < recipient_fee)) { //check for sufficient funds
        if(verbose) {
          cout << "Insufficient funds to process transaction " << current_transaction->transaction_ID << ".\n";
        }
        delete current_transaction;
      } else if((current_transaction->user == current_transaction->recepient) && ((u->second->balance < (current_transaction->fee + current_transaction->amount)))) {
        if(verbose) {
          cout << "Insufficient funds to process transaction " << current_transaction->transaction_ID << ".\n";
        }
        delete current_transaction;
      } else { //process transaction
        r->second->balance = r->second->balance + current_transaction->amount - recipient_fee;
        r->second->incoming_transactions.push_back(current_transaction);
        r->second->num_incoming_transaction++;
        if(r->second->incoming_transactions.size() > 10) {
          r->second->incoming_transactions.erase(r->second->incoming_transactions.begin());
        }
        u->second->balance = u->second->balance - current_transaction->amount - user_fee;
        u->second->outgoing_transactions.push_back(current_transaction);
        u->second->num_outgoing_transaction++;
        if(u->second->outgoing_transactions.size() > 10) {
          u->second->outgoing_transactions.erase(u->second->outgoing_transactions.begin());
        }
        transaction_masterlist.push_back(current_transaction);
        if(verbose) {
          cout << "Transaction executed at " << current_transaction->exec_date << ": $" << current_transaction->amount
            << " from " << current_transaction->user << " to " << current_transaction->recepient << ".\n";
        }
      }
    }

  }

  void transaction_between(string &first, string &second) {
    uint32_t index = 0;
    uint32_t count = 0;
    
    uint64_t lower_bound = remove_timestamp_colons(first);
    uint64_t upper_bound = remove_timestamp_colons(second);

    for(uint32_t i = 0; i < transaction_masterlist.size(); ++i) {
      if(transaction_masterlist[i]->exec_date >= lower_bound) {
        index = i;
        break;
      }
    }

    while((index < transaction_masterlist.size()) && (transaction_masterlist[index]->exec_date < upper_bound)) {
      count++;
      if(transaction_masterlist[index]->amount == 1) {
        cout << transaction_masterlist[index]->transaction_ID << ": " << transaction_masterlist[index]->user << " sent " <<
        transaction_masterlist[index]->amount << " dollar to " << transaction_masterlist[index]->recepient << " at " <<
          transaction_masterlist[index]->exec_date << ".\n";
      } else {
        cout << transaction_masterlist[index]->transaction_ID << ": " << transaction_masterlist[index]->user << " sent " <<
          transaction_masterlist[index]->amount << " dollars to " << transaction_masterlist[index]->recepient << " at " <<
            transaction_masterlist[index]->exec_date << ".\n";
      }
      index++;
    }

    if(count == 1) {
      cout << "There was " << count <<" transaction that was placed between time " << lower_bound << " to " << upper_bound << ".\n";
    } else {
      cout << "There were " << count <<" transactions that were placed between time " << lower_bound << " to " << upper_bound << ".\n";
    }
  }

  void revenue_between(string &first, string &second) {
    uint32_t index = 0;
    uint32_t revenue = 0;

    uint64_t lower_bound = remove_timestamp_colons(first);
    uint64_t upper_bound = remove_timestamp_colons(second);

    for(uint32_t i = 0; i < transaction_masterlist.size(); ++i) {
      if(transaction_masterlist[i]->exec_date >= lower_bound) {
        index = i;
        break;
      }
    }

    while((index < transaction_masterlist.size()) && (transaction_masterlist[index]->exec_date < upper_bound)) {
      revenue += transaction_masterlist[index]->fee;
      index++;
    }
    string total_time = regular_timestamp(lower_bound, upper_bound);

    //281Bank has collected 420 dollars in fees over 2 months 3 days 1 hour 22 seconds
    cout << "281Bank has collected " << revenue << " dollars in fees over" << total_time << ".\n";
  }

  string regular_timestamp(uint64_t &first, uint64_t &second) {
    uint64_t total_time = second - first;
    string time;

    //years
    uint64_t years = total_time / 10000000000;
    if(years == 1) {
      time += " " + to_string(years) + " year";
    } else if (years != 0) {
      time += " " + to_string(years) + " years";
    }
    total_time -= years * 10000000000;

    //months
    uint64_t months = total_time / 100000000;
    if(months == 1) {
      time += " " + to_string(months) + " month";
    } else if (months != 0) {
      time += " " + to_string(months) + " months";
    }
    total_time -= months * 100000000;

    //days
    uint64_t days = total_time / 1000000;
    if(days == 1) {
      time += " " + to_string(days) + " day";
    } else if (days != 0) {
      time += " " + to_string(days) + " days";
    }
    total_time -= days * 1000000;
    
    //hours
    uint64_t hours = total_time / 10000;
    if(hours == 1) {
      time += " " + to_string(hours) + " hour";
    } else if (hours != 0) {
      time += " " + to_string(hours) + " hours";
    }
    total_time -= hours * 10000;

    //minutes
    uint64_t minutes = total_time / 100;
    if(minutes == 1) {
      time += " " + to_string(minutes) + " minute";
    } else if (minutes != 0) {
      time += " " + to_string(minutes) + " minutes";
    }
    total_time -= minutes * 100;

    //seconds
    uint64_t seconds = total_time;
    if(seconds == 1) {
      time += " " + to_string(seconds) + " second";
    } else if (seconds != 0) {
      time += " " + to_string(seconds) + " seconds";
    }
    return time;
  }

  void print_customer_history(string &user_input) {
    auto it = user_masterlist.find(user_input);

    if(it == user_masterlist.end()) {
      cout << "User " << user_input << " does not exist.\n";
    } else {
      cout << "Customer " << user_input << " account summary:\n";
      cout << "Balance: $" << it->second->balance << '\n';
      cout << "Total # of transactions: " << it->second->num_incoming_transaction + it->second->num_outgoing_transaction << '\n';

      cout << "Incoming " << it->second->num_incoming_transaction << ":\n";
      for(auto t : it->second->incoming_transactions) {
        cout << t->transaction_ID << ": " << t->user << " sent ";
        if(t->amount == 1) {
          cout << "1 dollar ";
        } else {
          cout << t->amount << " dollars ";
        }
        cout << "to " << t->recepient << " at " << t->exec_date << ".\n";
      }

      cout << "Outgoing " << it->second->num_outgoing_transaction << ":\n";
      for(auto t : it->second->outgoing_transactions) {
        cout << t->transaction_ID << ": " << t->user << " sent ";
        if(t->amount == 1) {
          cout << "1 dollar ";
        } else {
          cout << t->amount << " dollars ";
        }
        cout << "to " << t->recepient << " at " << t->exec_date << ".\n";
      }
    }
  }

  void print_activity(string &time_input) {
    uint64_t first_input = remove_timestamp_colons(time_input);

    uint64_t lower_time = first_input / 1000000;
    lower_time *= 1000000;
    uint64_t upper_time = lower_time + 1000000;

    cout << "Summary of [" << lower_time << ", " << upper_time << "):\n";

    uint32_t index = 0;
    uint32_t revenue = 0;
    int count = 0;
    for(uint32_t i = 0; i < transaction_masterlist.size(); ++i) {
      if(transaction_masterlist[i]->exec_date >= lower_time) {
        index = i;
        break;
      }
    }

    if((lower_time < transaction_masterlist[transaction_masterlist.size() - 1]->exec_date) && (upper_time > transaction_masterlist[0]->exec_date)) {
    while((index < transaction_masterlist.size()) && (transaction_masterlist[index]->exec_date < upper_time)) {
      count++;
      revenue += transaction_masterlist[index]->fee;
      if(transaction_masterlist[index]->amount == 1) {
        cout << transaction_masterlist[index]->transaction_ID << ": " << transaction_masterlist[index]->user << " sent " <<
        transaction_masterlist[index]->amount << " dollar to " << transaction_masterlist[index]->recepient << " at " <<
          transaction_masterlist[index]->exec_date << ".\n";
      } else {
        cout << transaction_masterlist[index]->transaction_ID << ": " << transaction_masterlist[index]->user << " sent " <<
          transaction_masterlist[index]->amount << " dollars to " << transaction_masterlist[index]->recepient << " at " <<
            transaction_masterlist[index]->exec_date << ".\n";
      }
      index++;
    }
    }

    if(count == 1) {
      cout << "There was a total of " << count <<" transaction, 281Bank has collected " << revenue << " dollars in fees.\n";
    } else {
      cout << "There were a total of " << count <<" transactions, 281Bank has collected " << revenue << " dollars in fees.\n";
    }
  }

  void delete_all_transactions() {
    for(uint32_t x = 0; x < transaction_masterlist.size(); ++x) {
      delete transaction_masterlist[x];
    }
  }

  void delete_all_users() {
    for(auto it: user_masterlist) {
      delete it.second;
    }
    user_masterlist.clear();
  }
};

int main(int argc, char* argv[]) {
  std::ios_base::sync_with_stdio(false);
  program bank;
  int index = 0;
  int opt = 0;
  bank.verbose = false;
  ifstream reg_file;
  string input_file;

  while ((opt = getopt_long(argc, argv, "hvf:", redirect, &index)) != -1) {
    switch (opt) {
     case 'h': //work on the help line
        cout << "This program finds a path out of a puzzle if there is one\n";
        cout << "These are the valid command line arguments:\n";
        cout << "'-s' or '--statistics' will use a stack based path finding scheme\n";
        cout << "'-q' or '--queue' will use a queue based path finding scheme\n";
        cout << "'-o' or '--output' will indicate if you want a map or list output\n";
        cout << "'-h' or '--help' will print this message again and end the program\n";
        return 0;
        break;
      case 'v':
        bank.verbose = true;
        break;
      case 'f':
        input_file = string(optarg);
        break;
      default:
        cerr << "Error: Unknown option\n";
        return 1;
    }
  }

  reg_file.open(input_file);

  if(!reg_file.is_open()) {
    cout << "Registration file failed to open.\n";
    return 1;
  }
  
  //cin reads from the commands file
  //reg_file reads in from the registration file

  string reg_time;
  uint64_t time_nocolon;
  uint64_t prev_time = 0;
  string user_ID_input;
  uint32_t pin_input;
  uint32_t balance_input;
  uint32_t num_transaction = 0;

  while(reg_file) {
    string reg_time1;
    uint64_t time_nocolon1;
    string user_ID_input1;
    string pin_input_word1;
    uint32_t pin_input1;
    uint32_t balance_input1;

    getline(reg_file, reg_time1, '|');
    if((reg_time1 == "\n") || (reg_time1 == "") || (reg_time1 == " ")) {
      break;
    }
    time_nocolon1 = bank.remove_timestamp_colons(reg_time1);
    getline(reg_file, user_ID_input1, '|');
    getline(reg_file, pin_input_word1, '|');
    pin_input1 =  static_cast<uint32_t>(std::stoul(pin_input_word1));
    reg_file >> balance_input1;

    User* new_user = new User;
    new_user->reg_time = time_nocolon1;
    new_user->user_ID = user_ID_input1;
    new_user->pin = pin_input1;
    new_user->balance = balance_input1;
    new_user->active_session = false;
    new_user->num_incoming_transaction = 0;
    new_user->num_outgoing_transaction = 0;

    bank.user_masterlist.insert({new_user->user_ID, new_user});
  }

  bank.commands_stage = true;
  string command;
  string trash;
  string IP_address_input;
  
  while(bank.commands_stage) { //for all the commands
    cin >> command;
    if((command == "#") || (command[0] == '#')) { //check for comment
      getline( cin, trash);
    } else if(command == "login") {
      cin >> user_ID_input >> pin_input >> IP_address_input;

      bank.login(user_ID_input, pin_input, IP_address_input);
    } else if(command == "out") {
      cin >> user_ID_input >> IP_address_input;

      bank.logout(user_ID_input, IP_address_input);
    } else if(command == "place") {
      string reciever;
      char split;
      cin >> reg_time;
      time_nocolon = bank.remove_timestamp_colons(reg_time);

      if((prev_time != 0) && (time_nocolon < prev_time)) {
        cerr << "Invalid decreasing timestamp in 'place' command.\n";
        return 1;
      }

      cin >> IP_address_input >> user_ID_input >> reciever >> balance_input >> reg_time >> split;

      transaction* new_transaction = new transaction;
      new_transaction->amount = balance_input;
      new_transaction->exec_date = bank.remove_timestamp_colons(reg_time);
      new_transaction->place_time = time_nocolon;
      new_transaction->recepient = reciever;
      new_transaction->user = user_ID_input;

      bool pass = bank.check_transaction(*new_transaction, IP_address_input);

    if(pass) {
      prev_time = new_transaction->place_time;
      if(split == 's') {
        new_transaction->shared = true;
      } else {
        new_transaction->shared = false;
      }
      new_transaction->fee = bank.calculate_fee(new_transaction);
      new_transaction->transaction_ID = num_transaction;
      num_transaction++;

      if(bank.pending_transactions.empty()) {
        bank.pending_transactions.push(new_transaction);
      } else {
        while((!bank.pending_transactions.empty()) && (new_transaction->place_time >= bank.pending_transactions.top()->exec_date)) {
          bank.execute_transactions();
        }
        bank.pending_transactions.push(new_transaction);
      }

      if(bank.verbose) {
        cout << "Transaction placed at " << new_transaction->place_time << ": $" << new_transaction->amount << " from " << new_transaction->user
          << " to " << new_transaction->recepient << " at " << new_transaction->exec_date << ".\n";
      }

      } else {
        delete new_transaction;
      }
    } else if(command == "$$$") {
      bank.commands_stage = false;
      while(!bank.pending_transactions.empty()) {
        bank.execute_transactions();
      }
    }
  }
  while(cin) {
    string command_q;
    cin >> command_q;

    if(command_q == "l") {
      string first;
      string second;
      cin >> first >> second;

      bank.transaction_between(first, second);
    } else if(command_q == "r") {
      string first;
      string second;
      cin >> first >> second;

      bank.revenue_between(first, second);
    } else if (command_q == "h") {
      string user_ID_input;
      cin >> user_ID_input;

      bank.print_customer_history(user_ID_input);
    } else if(command_q == "s") {
      string time;
      cin >> time;

      bank.print_activity(time);
    }
  }
  
  bank.delete_all_transactions();
  bank.delete_all_users();
  return 0;
}
