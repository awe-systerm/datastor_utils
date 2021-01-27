//
// Created by root on 6/18/20.
//

#include <awe_log.h>
#include "algo_aio_device.h"

namespace ServerSan_Algo {

algo_aio_device::algo_aio_device(const string ser, const string path, unsigned long size_secs,
                                 libaio_device_service *aio_server) :
        aio_server_(aio_server),serial_num_(ser), path_(path), sector_num_(size_secs),
    aio_dev_(path, 4096, aio_server, std::bind(&algo_aio_device::req_done, this, std::placeholders::_1,
                                                     std::placeholders::_2)){}


unsigned long long int ServerSan_Algo::algo_aio_device::get_sector_num() const {
    return sector_num_;
}

string ServerSan_Algo::algo_aio_device::get_host_name(void) const {
    return serial_num_;
}

bool ServerSan_Algo::algo_aio_device::is_local() const {
    return false;
}

int ServerSan_Algo::algo_aio_device::open(void) {
    return aio_dev_.open();
}

void ServerSan_Algo::algo_aio_device::close(void) {
    aio_dev_.close();
}

void ServerSan_Algo::algo_aio_device::do_request(ServerSan_Algo::request_t *request) {
    AWE_MODULE_DEBUG("algo",
                     "do_request %p request %p : %s request",
                     this, request, request->to_json_obj().dumps().c_str());
    if( request->is_read()) {
        AWE_MODULE_DEBUG("algo",
                         "do_request %p request %p : %s request",
                         this, request, request->to_json_obj().dumps().c_str());
        aio_dev_.async_read(request->offset, request->len, request->buf, request);
    } else {
        AWE_MODULE_DEBUG("algo",
                         "do_request %p request %p : %s request",
                         this, request, request->to_json_obj().dumps().c_str());
        aio_dev_.async_write(request->offset, request->len, request->buf, request);
    }
}

void algo_aio_device::req_done(void *pri, int error) {
    AWE_MODULE_DEBUG("algo",
                     "req_done %p request %p : %s request",
                     this, pri, static_cast<ServerSan_Algo::request_t*>(pri)->to_json_obj().dumps().c_str());
    complete_request(static_cast<ServerSan_Algo::request_t*>(pri), error);
}

string algo_aio_device::get_device_id(void) const {
    return serial_num_;
}
}
