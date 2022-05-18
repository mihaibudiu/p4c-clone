typedef bit<1> s;
enum s e {
    value = 1w1
}

typedef bool t;
struct h {
    t f1;
    t f2;
}

parser MyParser1(in tuple<t, t> tt1, in h b) {
    state start {
        transition select(tt1) {
            b: state1;
            default: accept;
        }
    }
    state state1 {
        transition accept;
    }
}

