#include <core.p4>
#include <bmv2/psa.p4>

typedef bit<48> EthernetAddress;
header ethernet_t {
    EthernetAddress dstAddr;
    EthernetAddress srcAddr;
    bit<16>         etherType;
}

header ipv4_t {
    bit<4>  version;
    bit<4>  ihl;
    bit<8>  diffserv;
    bit<16> totalLen;
    bit<16> identification;
    bit<3>  flags;
    bit<13> fragOffset;
    bit<8>  ttl;
    bit<8>  protocol;
    bit<16> hdrChecksum;
    bit<32> srcAddr;
    bit<32> dstAddr;
}

header tcp_t {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<32> seqNo;
    bit<32> ackNo;
    bit<4>  dataOffset;
    bit<3>  res;
    bit<3>  ecn;
    bit<6>  ctrl;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgentPtr;
}

struct empty_metadata_t {
}

struct metadata {
    bit<16> data;
    bit<8>  tmpMask;
}

struct headers {
    ethernet_t ethernet;
    ipv4_t     ipv4;
    tcp_t      tcp;
}

parser IngressParserImpl(packet_in buffer, out headers hdr, inout metadata user_meta, in psa_ingress_parser_input_metadata_t istd, in empty_metadata_t resubmit_meta, in empty_metadata_t recirculate_meta) {
    state start {
        buffer.extract<ethernet_t>(hdr.ethernet);
        transition select(hdr.ethernet.etherType) {
            16w0x800 &&& 16w0xf00: parse_ipv4;
            default: accept;
        }
    }
    state parse_ipv4 {
        buffer.extract<ipv4_t>(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            8w4 &&& 8w252: parse_tcp;
            default: accept;
        }
    }
    state parse_tcp {
        buffer.extract<tcp_t>(hdr.tcp);
        transition accept;
    }
}

control ingress(inout headers hdr, inout metadata user_meta, in psa_ingress_input_metadata_t istd, inout psa_ingress_output_metadata_t ostd) {
    @name("ingress.tmpMask") bit<16> tmpMask_1;
    @noWarn("unused") @name(".NoAction") action NoAction_1() {
    }
    @name("ingress.execute") action execute_1() {
        user_meta.data = tmpMask_1;
    }
    @name("ingress.tbl") table tbl_0 {
        key = {
            hdr.ethernet.srcAddr: range @name("hdr.ethernet.srcAddr") ;
            hdr.ethernet.dstAddr: exact @name("hdr.ethernet.dstAddr") ;
        }
        actions = {
            NoAction_1();
            execute_1();
        }
        const default_action = NoAction_1();
    }
    @hidden action psaexamplerangematch91() {
        tmpMask_1 = 16w1;
    }
    @hidden table tbl_psaexamplerangematch91 {
        actions = {
            psaexamplerangematch91();
        }
        const default_action = psaexamplerangematch91();
    }
    apply {
        tbl_psaexamplerangematch91.apply();
        tbl_0.apply();
    }
}

parser EgressParserImpl(packet_in buffer, out headers hdr, inout metadata user_meta, in psa_egress_parser_input_metadata_t istd, in empty_metadata_t normal_meta, in empty_metadata_t clone_i2e_meta, in empty_metadata_t clone_e2e_meta) {
    state start {
        buffer.extract<ethernet_t>(hdr.ethernet);
        transition select(hdr.ethernet.etherType) {
            16w0x80 &&& 16w0xc0: parse_ipv4;
            default: accept;
        }
    }
    state parse_ipv4 {
        buffer.extract<ipv4_t>(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            8w16 &&& 8w248: parse_tcp;
            default: accept;
        }
    }
    state parse_tcp {
        buffer.extract<tcp_t>(hdr.tcp);
        transition accept;
    }
}

control egress(inout headers hdr, inout metadata user_meta, in psa_egress_input_metadata_t istd, inout psa_egress_output_metadata_t ostd) {
    apply {
    }
}

control IngressDeparserImpl(packet_out packet, out empty_metadata_t clone_i2e_meta, out empty_metadata_t resubmit_meta, out empty_metadata_t normal_meta, inout headers hdr, in metadata meta, in psa_ingress_output_metadata_t istd) {
    @hidden action psaexamplerangematch153() {
        packet.emit<ethernet_t>(hdr.ethernet);
        packet.emit<ipv4_t>(hdr.ipv4);
        packet.emit<tcp_t>(hdr.tcp);
    }
    @hidden table tbl_psaexamplerangematch153 {
        actions = {
            psaexamplerangematch153();
        }
        const default_action = psaexamplerangematch153();
    }
    apply {
        tbl_psaexamplerangematch153.apply();
    }
}

control EgressDeparserImpl(packet_out packet, out empty_metadata_t clone_e2e_meta, out empty_metadata_t recirculate_meta, inout headers hdr, in metadata meta, in psa_egress_output_metadata_t istd, in psa_egress_deparser_input_metadata_t edstd) {
    @hidden action psaexamplerangematch169() {
        packet.emit<ethernet_t>(hdr.ethernet);
        packet.emit<ipv4_t>(hdr.ipv4);
        packet.emit<tcp_t>(hdr.tcp);
    }
    @hidden table tbl_psaexamplerangematch169 {
        actions = {
            psaexamplerangematch169();
        }
        const default_action = psaexamplerangematch169();
    }
    apply {
        tbl_psaexamplerangematch169.apply();
    }
}

IngressPipeline<headers, metadata, empty_metadata_t, empty_metadata_t, empty_metadata_t, empty_metadata_t>(IngressParserImpl(), ingress(), IngressDeparserImpl()) ip;

EgressPipeline<headers, metadata, empty_metadata_t, empty_metadata_t, empty_metadata_t, empty_metadata_t>(EgressParserImpl(), egress(), EgressDeparserImpl()) ep;

PSA_Switch<headers, metadata, headers, metadata, empty_metadata_t, empty_metadata_t, empty_metadata_t, empty_metadata_t, empty_metadata_t>(ip, PacketReplicationEngine(), ep, BufferingQueueingEngine()) main;

