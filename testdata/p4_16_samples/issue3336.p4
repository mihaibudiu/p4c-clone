typedef bit s;

enum s e
{
  value = 1
}
typedef bool t;

struct h
{
  t f1;
  t f2;
}

parser MyParser1(in tuple<t, t> tt1, in h b) {
    state start {
        transition select(tt1) {
                b: state1;
                _: accept;
        }
    }
    state state1 {
        transition accept;
    }
}
