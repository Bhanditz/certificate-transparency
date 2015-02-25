#include <atomic>
#include <event2/thread.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "util/etcd.h"
#include "util/masterelection.h"

namespace libevent = cert_trans::libevent;

using cert_trans::EtcdClient;
using cert_trans::MasterElection;
using cert_trans::UrlFetcher;
using std::make_shared;
using std::shared_ptr;

DEFINE_string(etcd, "127.0.0.1", "etcd server address");
DEFINE_int32(etcd_port, 4001, "etcd server port");
DEFINE_string(proposal_dir, "/master", "path to watch");
DEFINE_string(node_id, "", "unique node id.");


std::atomic<bool> running_;


void EventPump(const shared_ptr<libevent::Base>& event_base) {
  while (true) {
    {
      if (!running_.load()) {
        return;
      }
    }
    event_base->DispatchOnce();
  }
}


int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  evthread_use_pthreads();
  CHECK(!FLAGS_node_id.empty()) << "Must set --node_id";

  running_.store(true);
  const shared_ptr<libevent::Base> event_base(make_shared<libevent::Base>());
  UrlFetcher fetcher(event_base.get());

  EtcdClient etcd(event_base, &fetcher, FLAGS_etcd, FLAGS_etcd_port);
  MasterElection election(event_base, &etcd, FLAGS_proposal_dir,
                          FLAGS_node_id);
  election.StartElection();

  std::thread pump(EventPump, event_base);

  LOG(INFO) << "Waiting to become master...";
  election.WaitToBecomeMaster();
  LOG(INFO) << "I'm the boss!";

  sleep(10);

  LOG(INFO) << "Giving it all up and going fishing instead...";
  election.StopElection();
  LOG(INFO) << "Gone fishin'.";

  running_.store(false);
  pump.join();

  return 0;
}
