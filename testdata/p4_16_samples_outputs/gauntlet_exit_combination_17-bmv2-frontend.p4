#include <core.p4>
#define V1MODEL_VERSION 20180101
#include <v1model.p4>

header ethernet_t {
    bit<48> dst_addr;
    bit<48> src_addr;
    bit<16> eth_type;
}

struct Headers {
    ethernet_t eth_hdr;
}

struct Meta {
}

parser p(packet_in pkt, out Headers hdr, inout Meta m, inout standard_metadata_t sm) {
    state start {
        pkt.extract<ethernet_t>(hdr.eth_hdr);
        transition accept;
    }
}

control ingress(inout Headers h, inout Meta m, inout standard_metadata_t sm) {
    @name("ingress.hasReturned") bool hasReturned;
    @noWarn("unused") @name(".NoAction") action NoAction_1() {
    }
    @noWarn("unused") @name(".NoAction") action NoAction_2() {
    }
    @name("ingress.dummy_action") action dummy_action() {
    }
    @name("ingress.dummy_action") action dummy_action_1() {
    }
    @name("ingress.simple_table_1") table simple_table {
        key = {
            48w1: exact @name("key") ;
        }
        actions = {
            dummy_action();
            @defaultonly NoAction_1();
        }
        const default_action = NoAction_1();
    }
    @name("ingress.simple_table_2") table simple_table_0 {
        key = {
            48w1: exact @name("key") ;
        }
        actions = {
            dummy_action_1();
            @defaultonly NoAction_2();
        }
        const default_action = NoAction_2();
    }
    apply {
        hasReturned = false;
        switch (simple_table.apply().action_run) {
            dummy_action: {
                switch (simple_table_0.apply().action_run) {
                    dummy_action_1: {
                        h.eth_hdr.src_addr = 48w4;
                        hasReturned = true;
                    }
                    default: {
                    }
                }
            }
            default: {
            }
        }
        if (hasReturned) {
            ;
        } else {
            exit;
        }
    }
}

control vrfy(inout Headers h, inout Meta m) {
    apply {
    }
}

control update(inout Headers h, inout Meta m) {
    apply {
    }
}

control egress(inout Headers h, inout Meta m, inout standard_metadata_t sm) {
    apply {
    }
}

control deparser(packet_out pkt, in Headers h) {
    apply {
        pkt.emit<Headers>(h);
    }
}

V1Switch<Headers, Meta>(p(), vrfy(), ingress(), egress(), update(), deparser()) main;

