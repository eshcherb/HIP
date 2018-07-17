// automatically generated sources
#ifndef _HIP_CBAPI_H
#define _HIP_CBAPI_H

#include <hip/hip_cbstr.h>

#include <atomic>
#include <mutex>

// Callbacks spawner instantiation
#define HIP_CALLBACKS_INSTANCE \
  hip_cb_table_t api_callbacks_table_t::callbacks_table{}; \
  api_callbacks_table_t::mutex_t api_callbacks_table_t::mutex_;

// Set HIP activity/callback macros
#define HIP_SET_ACTIVITY api_callbacks_table_t::set_activity
#define HIP_SET_CALLBACK api_callbacks_table_t::set_callback

// HIP API callbacks spawner object macro
#define CB_SPAWNER_OBJECT(CB_ID) \
  hip_cb_data_t cb_data{}; \
  INIT_CB_ARGS_DATA(CB_ID, cb_data); \
  api_callbacks_spawner_t __api_tracer(HIP_API_ID_##CB_ID, cb_data);

// HIP API callbacks table
struct hip_cb_table_entry_t {
  volatile std::atomic<bool> sync;
  volatile std::atomic<uint32_t> sem;
  hip_cb_act_t act;
  void* a_arg;
  hip_cb_fun_t fun;
  void* arg;
};

struct hip_cb_table_t {
  volatile hip_cb_table_entry_t arr[HIP_API_ID_NUMBER];
};

enum { HIP_DOMAIN_ID = 1 };

class api_callbacks_table_t {
 public:
  typedef std::recursive_mutex mutex_t;

  static bool set_activity(uint32_t id, hip_cb_act_t fun, void* arg) {
    std::lock_guard<mutex_t> lock(mutex_);
    bool ret = true;
    if (id == HIP_API_ID_ANY) {
      for (unsigned i = 0; i < HIP_API_ID_NUMBER; ++i) set_activity(i, fun, arg);
    } else if (id < HIP_API_ID_NUMBER) {
      cb_sync(id);
      callbacks_table.arr[id].act = fun;
      callbacks_table.arr[id].a_arg = arg;
      cb_release(id);
    } else {
      ret = false;
    }
    return ret;
  }

  static bool set_callback(uint32_t id, hip_cb_fun_t fun, void* arg) {
    std::lock_guard<mutex_t> lock(mutex_);
    bool ret = true;
    if (id == HIP_API_ID_ANY) {
      for (unsigned i = 0; i < HIP_API_ID_NUMBER; ++i) set_callback(i, fun, arg);
    } else if (id < HIP_API_ID_NUMBER) {
      cb_sync(id);
      callbacks_table.arr[id].fun = fun;
      callbacks_table.arr[id].arg = arg;
      cb_release(id);
    } else {
      ret = false;
    }
    return ret;
  }

 protected:
  inline static volatile hip_cb_table_entry_t& entry(const uint32_t& id) {
    return callbacks_table.arr[id];
  }

  inline static void cb_sync(const uint32_t& id) {
    entry(id).sync.store(true);
    while (entry(id).sem.load() != 0) {}
  }

  inline static void cb_release(const uint32_t& id) {
    entry(id).sync.store(false);
  }

  inline static void sem_increment(const uint32_t& id) {
    const uint32_t prev = entry(id).sem.fetch_add(1);
    if (prev == UINT32_MAX) {
      std::cerr << "sem overflow id = " << id << std::endl << std::flush;
      abort();
    }
  }

  inline static void sem_decrement(const uint32_t& id) {
    const uint32_t prev = entry(id).sem.fetch_sub(1);
    if (prev == 0) {
      std::cerr << "sem corrupted id = " << id << std::endl << std::flush;
      abort();
    }
  }

  static void sync_wait(const uint32_t& id) {
    sem_decrement(id);
    while (entry(id).sync.load() == true) {}
    sem_increment(id);
  }

  static void sem_sync(const uint32_t& id) {
    sem_increment(id);
    if (entry(id).sync.load() == true) sync_wait(id);
  }

  static void sem_release(const uint32_t& id) {
    sem_decrement(id);
  }

 private:
  static mutex_t mutex_;
  static hip_cb_table_t callbacks_table;
};

class api_callbacks_spawner_t : public api_callbacks_table_t {
 public:
  api_callbacks_spawner_t(const hip_cb_id_t& cid, hip_cb_data_t& cb_data) :
    cid_(cid),
    cb_data_(cb_data),
    record_({})
  {
    if (cid_ >= HIP_API_ID_NUMBER) {
      fprintf(stderr, "HIP %s bad id %d\n", __FUNCTION__, cid_);
      abort();
    }
    sem_sync(cid_);

    act = entry(cid_).act;
    a_arg = entry(cid_).a_arg;
    fun = entry(cid_).fun;
    arg = entry(cid_).arg;

    cb_data_.phase = 0;
    if (act != NULL) act(cid_, &record_, &cb_data_, a_arg);
    if (fun != NULL) fun(HIP_DOMAIN_ID, cid_, &cb_data_, arg);
  }

  ~api_callbacks_spawner_t() {
    cb_data_.phase = 1;
    if (act != NULL) act(cid_, &record_, &cb_data_, a_arg);
    if (fun != NULL) fun(HIP_DOMAIN_ID, cid_, &cb_data_, arg);

    sem_release(cid_);
  }

 private:
  const hip_cb_id_t cid_;
  hip_cb_data_t& cb_data_;
  hip_act_record_t record_;

  hip_cb_act_t act;
  void* a_arg;
  hip_cb_fun_t fun;
  void* arg;
};

#endif  // _HIP_CBAPI_H
