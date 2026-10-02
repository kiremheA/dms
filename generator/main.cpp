#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
using namespace std;

mt19937 rng;

int rnd(int n) { return rng() % n; }

int skewed(int n) {
    double u = rng() / 4294967296.0;
    return (int)(u * u * n);
}

vector<string> readLines(const string& path) {
    ifstream f(path);
    if (!f) { cerr << "Не открыть файл " << path << "\n"; exit(1); }
    vector<string> v;
    string s;
    while (getline(f, s)) {
        if (!s.empty() && s.back() == '\r') s.pop_back();
        if (!s.empty()) v.push_back(s);
    }
    return v;
}

vector<string> split(const string& s, char d) {
    vector<string> v;
    string cur;
    for (char c : s) {
        if (c == d) { v.push_back(cur); cur.clear(); }
        else cur += c;
    }
    v.push_back(cur);
    return v;
}

string ts(long long t) {
    time_t tt = t;
    char buf[32];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S+00", gmtime(&tt));
    return buf;
}

string num(double x) {
    char buf[32];
    snprintf(buf, sizeof buf, "%.2f", x);
    return buf;
}

void check(bool ok, const string& msg) {
    if (!ok) { cerr << "Ошибка во входных данных: " << msg << "\n"; exit(1); }
}

enum { QUEUED, DIAGNOSIS, APPROVAL, REPAIR, PART, READY, ISSUED, CANCELLED };
const char* STATUS[] = {"queued", "diagnosis", "awaiting_approval", "in_repair",
                        "awaiting_part", "ready_for_pickup", "issued", "cancelled"};

int recentStatus() {
    int w[] = {20, 15, 15, 20, 5, 15, 8, 2};
    int r = rnd(100);
    for (int s = 0; s < 8; s++) {
        if (r < w[s]) return s;
        r -= w[s];
    }
    return ISSUED;
}

int main(int argc, char** argv) {
    if (argc < 4) { cerr << "usage: gen <seed> <out_dir> <dev|load> [data_dir]\n"; return 1; }
    rng.seed(atoi(argv[1]));
    string out = argv[2];
    string mode = argv[3];
    string in = argc > 4 ? argv[4] : "data";
    check(mode == "dev" || mode == "load", "режим должен быть dev или load");

    int N = (mode == "load") ? 600000 : 15000;
    int nCust = N / 4, nDev = N / 2;

    vector<string> cats = readLines(in + "/categories.txt");
    vector<string> norms = readLines(in + "/norms.txt");
    vector<string> svcs = readLines(in + "/services.txt");
    vector<string> parts = readLines(in + "/parts.txt");
    vector<string> staff = readLines(in + "/staff.txt");
    vector<string> firstNames = readLines(in + "/first_names.txt");
    vector<string> lastNames = readLines(in + "/last_names.txt");
    vector<string> patronymics = readLines(in + "/patronymics.txt");
    vector<string> malfunctions = readLines(in + "/malfunctions.txt");
    vector<string> findings = readLines(in + "/findings.txt");

    ofstream fCat(out + "/device_category.csv"), fNorm(out + "/repair_norm.csv"),
        fSvc(out + "/work_service.csv"), fStaff(out + "/staff.csv"),
        fSpec(out + "/master_specialization.csv"), fPart(out + "/part.csv"),
        fCust(out + "/customer.csv"), fDev(out + "/device.csv"),
        fOrd(out + "/service_order.csv"), fHist(out + "/order_status_history.csv"),
        fMal(out + "/malfunction.csv"), fAsg(out + "/master_assignment.csv"),
        fDiag(out + "/diagnosis.csv"), fEst(out + "/estimate.csv"),
        fVer(out + "/estimate_version.csv"), fLine(out + "/estimate_line.csv"),
        fWork(out + "/work.csv"), fInv(out + "/invoice.csv"),
        fPay(out + "/payment.csv"), fMove(out + "/part_movement.csv");
    check(fCat.is_open(), "папка " + out + " не существует");

    int nCat = cats.size();
    vector<int> warranty(nCat + 1), diagDays(nCat + 1);
    for (int i = 0; i < nCat; i++) {
        vector<string> f = split(cats[i], ';');
        warranty[i + 1] = stoi(f[1]);
        diagDays[i + 1] = stoi(f[2]);
        fCat << i + 1 << ';' << cats[i] << '\n';
    }

    vector<vector<int>> normId(nCat + 1, vector<int>(2)), normDays(nCat + 1, vector<int>(2));
    for (int i = 0; i < (int)norms.size(); i++) {
        vector<string> f = split(norms[i], ';');
        int c = stoi(f[0]);
        int x = (f[1] == "complex");
        normId[c][x] = i + 1;
        normDays[c][x] = stoi(f[2]);
        fNorm << i + 1 << ';' << norms[i] << '\n';
    }

    vector<double> svcPrice(svcs.size() + 1);
    vector<vector<int>> svcByCat(nCat + 1);
    for (int i = 0; i < (int)svcs.size(); i++) {
        vector<string> f = split(svcs[i], ';');
        svcPrice[i + 1] = stod(f[2]);
        svcByCat[stoi(f[0])].push_back(i + 1);
        fSvc << i + 1 << ';' << svcs[i] << '\n';
    }

    int nPart = parts.size();
    vector<double> partPrice(nPart + 1);
    for (int i = 0; i < nPart; i++) {
        partPrice[i + 1] = stod(split(parts[i], ';')[2]);
        fPart << i + 1 << ';' << parts[i] << '\n';
    }

    int nStaff = staff.size();
    vector<int> dispatchers;
    vector<vector<int>> mastersByCat(nCat + 1);
    vector<int> masterBusy(nStaff + 1, 0);
    for (int i = 0; i < nStaff; i++) {
        vector<string> f = split(staff[i], ';');
        int id = i + 1;
        fStaff << id << ';' << f[0] << ';' << f[1] << ";\n";
        if (f[1] == "dispatcher") dispatchers.push_back(id);
        if (f[1] == "master") {
            for (const string& c : split(f[2], ',')) {
                mastersByCat[stoi(c)].push_back(id);
                fSpec << id << ';' << c << '\n';
            }
        }
    }

    check(!dispatchers.empty(), "в staff.txt нет диспетчера");
    for (int c = 1; c <= nCat; c++) {
        check(normId[c][0] && normId[c][1], "нет нормативов simple и complex у категории " + to_string(c));
        check(svcByCat[c].size() >= 3, "меньше 3 услуг у категории " + to_string(c));
        check(!mastersByCat[c].empty(), "нет мастера со специализацией " + to_string(c));
    }

    for (int c = 1; c <= nCust; c++) {
        string last = lastNames[skewed(lastNames.size())];
        string first = firstNames[skewed(firstNames.size())];
        string pat = patronymics[skewed(patronymics.size())];
        fCust << c << ';' << last << ' ' << first << ' ' << pat << ";+7" << 9000000000LL + c << '\n';
    }

    vector<int> devCat(nDev + 1);
    for (int d = 1; d <= nDev; d++) {
        devCat[d] = 1 + skewed(nCat);
        fDev << d << ';' << 1 + skewed(nCust) << ';' << devCat[d] << ";SN" << 10000000 + d << ";{}\n";
    }

    const long long T0 = 1735689600;
    const long long T1 = 1788220800;
    const long long RECENT = T1 - 15 * 86400;

    vector<long long> deviceFree(nDev + 1, 0);
    vector<int> lastIssued(nDev + 1, 0);
    int hid = 0, lid = 0, wid = 0, mid = 0;
    int live = 0;

    for (int p = 1; p <= nPart; p++)
        fMove << ++mid << ';' << p << ";;receipt;" << num(2.0 * N) << ';' << num(partPrice[p]) << ';' << ts(T0) << ";;\n";

    for (int o = 1; o <= N; o++) {
        long long created = T0 + (T1 - T0) * (o - 1) / N;

        int d;
        do { d = 1 + rnd(nDev); } while (deviceFree[d] > created);
        int cat = devCat[d];
        int orig = (lastIssued[d] && created - deviceFree[d] <= (long long)warranty[cat] * 86400 && rnd(4) == 0) ? lastIssued[d] : 0;
        int disp = dispatchers[rnd(dispatchers.size())];

        int st;
        if (created >= RECENT) st = recentStatus();
        else st = (rnd(100) < 8) ? CANCELLED : ISSUED;

        const vector<int>& ms = mastersByCat[cat];
        int master = 0;
        if (st >= DIAGNOSIS && st <= PART) {
            int s = skewed(ms.size());
            for (int k = 0; k < (int)ms.size() && master == 0; k++) {
                int m = ms[(s + k) % ms.size()];
                if (!masterBusy[m]) master = m;
            }
            if (master == 0) st = QUEUED;
            else {
                masterBusy[master] = 1;
                st = DIAGNOSIS + live++ % 4;
            }
        } else if (st == READY || st == ISSUED) {
            master = ms[skewed(ms.size())];
        }

        const vector<int>& sv = svcByCat[cat];
        int nLines = 1 + rnd(3);
        int first = skewed(sv.size());
        int lineSvc[3];
        double total = 0;
        for (int k = 0; k < nLines; k++) {
            lineSvc[k] = sv[(first + k) % sv.size()];
            total += svcPrice[lineSvc[k]];
        }
        int part = 0;
        double partQty = 0;
        if (st == PART || rnd(100) < 30) {
            part = 1 + skewed(nPart);
            partQty = 1 + rnd(2);
            total += partQty * partPrice[part];
        }
        int cx = (rnd(100) < 70) ? 0 : 1;

        int path[8], n = 0;
        if (st == CANCELLED) {
            path[n++] = QUEUED;
            path[n++] = CANCELLED;
        } else {
            for (int k = 0; k <= st; k++)
                if (k != PART || st == PART) path[n++] = k;
        }
        long long when[8] = {};
        long long t = created;
        for (int j = 0; j < n; j++) {
            when[path[j]] = t;
            fHist << ++hid << ';' << o << ';' << STATUS[path[j]] << ';' << ts(t) << ';' << disp << '\n';
            long long step = (1 + rnd(24)) * 3600;
            if (rnd(10) == 0) step += rnd(240) * 3600;
            t += min(step, (T1 - t) / (n - j));
        }
        deviceFree[d] = (st == ISSUED || st == CANCELLED) ? when[st] : (1LL << 62);
        lastIssued[d] = (st == ISSUED && !orig) ? o : 0;

        bool diagnosed = (st >= APPROVAL && st <= ISSUED);
        long long deadline = created + (long long)diagDays[cat] * 86400;
        if (diagnosed) deadline += (long long)normDays[cat][cx] * 86400;
        fOrd << o << ';' << d << ';' << disp << ';' << (orig ? "warranty" : "regular") << ';' << (orig ? to_string(orig) : "") << ';' << STATUS[st] << ';' << warranty[cat]
             << ";{};" << ts(created) << ';' << ts(deadline) << '\n';

        fMal << o << ';' << o << ';' << malfunctions[skewed(malfunctions.size())] << '\n';

        if (st >= DIAGNOSIS && st <= ISSUED) {
            fAsg << o << ';' << o << ';' << master << ';' << ts(when[DIAGNOSIS]) << ';';
            if (st >= READY) fAsg << ts(when[READY]);
            fAsg << ";\n";
        }

        if (diagnosed) {
            fDiag << o << ';' << o << ';' << o << ';' << normId[cat][cx] << ';'
                  << findings[skewed(findings.size())] << ';' << ts(when[APPROVAL]) << '\n';

            bool agreed = (st >= REPAIR);
            fEst << o << ';' << o << '\n';
            fVer << o << ';' << o << ";1;" << num(total) << ';' << (agreed ? "agreed" : "draft")
                 << ';' << ts(when[APPROVAL]) << ';';
            if (agreed) fVer << ts(when[REPAIR]);
            fVer << '\n';
            for (int k = 0; k < nLines; k++)
                fLine << ++lid << ';' << o << ";service;" << lineSvc[k] << ";;1.00;" << num(svcPrice[lineSvc[k]]) << '\n';
            if (part)
                fLine << ++lid << ';' << o << ";part;;" << part << ';' << num(partQty) << ';' << num(partPrice[part]) << '\n';
        }

        if (st == READY || st == ISSUED) {
            for (int k = 0; k < nLines; k++)
                fWork << ++wid << ';' << o << ';' << o << ';' << lineSvc[k] << ';' << num(svcPrice[lineSvc[k]])
                      << ';' << ts(when[READY]) << '\n';
            if (part)
                fMove << ++mid << ';' << part << ';' << o << ";writeoff;" << num(partQty) << ';'
                      << num(partPrice[part]) << ';' << ts(when[READY]) << ";;\n";
            if (!orig) {
                fInv << o << ';' << o << ';' << num(total) << ';' << ts(when[READY]) << '\n';
                if (st == ISSUED)
                    fPay << o << ';' << o << ';' << num(total) << ';' << ts(when[ISSUED]) << ';' << disp << '\n';
            }
        }
    }

    ofstream fSql(out + "/setval.sql");
    for (const char* tbl : {"device_category", "repair_norm", "work_service", "staff", "customer", "device",
                            "service_order", "order_status_history", "malfunction", "master_assignment",
                            "diagnosis", "estimate", "estimate_version", "part", "estimate_line",
                            "part_movement", "work", "invoice", "payment"})
        fSql << "select setval(pg_get_serial_sequence('" << tbl << "', 'id'), max(id)) from " << tbl << ";\n";

    cout << "service_order: " << N << ", order_status_history: " << hid << "\n";
    return 0;
}