#define SCHEDULER_V7_STAGE 4
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <variant>
#include <vector>
enum class bl {
Edge,
Cloud
};
struct bt {
bl type;
int dl;
};
struct aH {
int remote;
int rid;
};
struct aL {
int dj;
int dk;
int remote;
int rid;
};
struct aZ {
int remote;
int rid;
};
struct aM {
std::vector<int> rids;
};
struct be {
int remote;
std::vector<int> rids;
};
struct bf {
std::vector<int> rids;
};
using cy = std::variant<
aH,
aL,
aZ,
aM,
be,
bf
>;
struct ad {
bt server;
cy task;
};
struct cB {
int rid;
int bg;
};
struct cC {
bt server;
cy task;
double duration;
};
enum class af {
Up,
Down
};
enum class aY {
cQ,
Decode
};
struct aS {
af au;
int remote;
std::int64_t size_bytes;
aY stage;
std::vector < int > rids;
};
struct cD {
int rid;
};
using cz = std::variant<
cB,
cC,
aS,
cD
>;
struct cA {
double timestamp;
std::vector < cz > events;
};
std::optional < cA > db(std::istream& input);
void de(std::ostream& output, const std::vector<ad>& bm);
#include <iosfwd>
#include <cstdint>
struct ai {
double SLO1;
double SLO2;
double tp_UB;
double tp_base;
double dist_base;
double w_tp;
double w_c;
double S;
double dh;
double di;
int K;
int dg;
int ac;
};
double transferTime(const ai& config, std::int64_t len);
ai cY(std::istream& input);
#include <iosfwd>
#include <vector>
struct bC {
int av;
double cT;
double cU;
double cV;
double cW;
double bB;
double cX;
};
struct bd {
std::vector<bC> rows;
};
struct TimingPoint {
int av;
double duration;
};
struct bk {
std::vector < TimingPoint > points;
};
struct ax {
bk cT;
bk cU;
bk cV;
bk cW;
bk bB;
bk cX;
};
bd cZ(std::istream& input);
ax da(const bd& table);
double aF(const bk& curve, int size);
#include <cstdint>
#include <deque>
#include <vector>
struct ag {
af au;
int remote;
aY stage;
std::int64_t length;
std::vector<int> rids;
};
struct bO {
bt server;
cy task;
double du;
std::uint64_t order;
std::vector<ag> aB;
};
struct bP {
ag az;
double queued_at;
double starts_at;
double completes_at;
};
struct br {
double bN = 0.0;
std::deque<bP> committed;
};
struct bw {
br up;
br down;
std::vector<bO> bq;
std::uint64_t bI = 0;
int dt = 0;
double ds = 0.0;
};
#include <cstddef>
#include <optional>
#include <vector>
enum class aa {
cE,
bA,
cF,
bE,
bz,
cG,
cH,
cI,
ay,
cJ,
cK,
bn,
cL,
cM,
cN,
cO,
cP
};
struct cw {
int bg;
double by;
std::optional<int> remote;
int aN = 0;
int dm = 0;
std::optional<double> aA;
aa state;
};
struct cx  {
bool busy = false;
};
struct ab {
double aG = 0.0;
double dn = 0.0;
int dp = 0;
double dq = 0.0;
int dr = 0;
cx edge;
std::vector<cx> clouds;
std::vector<cw> aw;
bw links;
explicit ab(int cloud_count)
: clouds(static_cast<std::size_t>(cloud_count)) {}
};
void dc(ab& world, const cA& frame, int ac);
void dd(ab& world, const ad& ae, int ac);
#include <functional>
struct ab;
using aJ = std::function<int(const ab&, int)>;
#include <cstddef>
#include <vector>
struct aq {
std::vector<int> pre_by_ready_count;
std::vector<int> proc_by_ready_count;
std::vector<int> post_by_ready_count;
};
aq buildDecodeBatchPolicy(const ax& curves, double aE, int max_batch_size = 4096);
int aI(const aq& policy, aa state, std::size_t ready_count);
std::vector<ad> bc(
const ab& world, int ac, const aq& batch_policy);
std::vector<ad> bc(
const ab& world,
int ac,
const aq& batch_policy,
const aJ& ak);
#include <optional>
#include <vector>
enum class aj {
cQ,
cR,
cS,
cP
};
struct an {
aq bJ;
int ao;
double slo_tdr;
double slo_tpot;
double waiting_weight;
bool prefill_warmup;
};
aj aD(const cw& request);
int countActiveHotSet(const ab& world);
an buildScoreAwareSchedulerConfig(
const ai& system,
const ax& curves,
std::optional<int> ao = std::nullopt);
std::vector<ad> aQ(
const ab& world, int ac, const an& config);
std::vector<ad> aQ(
const ab& world,
int ac,
const an& config,
const aJ& ak);
#include <vector>
enum class ar {
HardSlo,
Waiting,
Balanced,
Throughput
};
struct as {
aq bv;
aq throughput_decode_batches;
an aV;
ar regime;
int preferred_decode_batch_size;
int bG;
int ao;
double estimated_decode_capacity;
double throughput_target;
};
ar classifyScoreRegime(const ai& system);
double estimateDecodeCapacity(
const ai& system, const ax& curves, int bh);
as buildAdaptiveSchedulerConfig(
const ai& system, const ax& curves);
std::vector<ad> bo(
const ab& world, int ac, const as& config);
std::vector<ad> bo(
const ab& world,
int ac,
const as& config,
const aJ& ak);
#include <vector>
struct al {
as adaptive;
bk prefill_proc_curve;
double aE;
double bu;
int ac;
};
al bK(
const ai& system, const ax& curves);
std::vector<double> estimateCloudWorkloads(
const ab& world, const al& config);
std::vector<double> estimatePlacementScores(
const ab& world, int rid, const al& config);
int chooseLoadAwareRemote(
const ab& world, int rid, const al& config);
std::vector<ad> chooseMultiprocessorAssignments(
const ab& world, int ac, const al& config);
#include <vector>
struct bT {
bool decode_locality = true;
bool prefill_up_admission = true;
bool prefill_down_admission = true;
};
struct ah {
al multiprocessor;
ai system;
ax curves;
bT features;
};
ah bX(
const ai& system,
const ax& curves,
bT features = {});
std::vector<ag> bY(
const ab& world, const cy& task, int ac);
double bs(
const ab& world, const cy& task, const ax& curves, int ac);
void bU(
ab& world,
const ad& ae,
const ai& system,
const ax& curves);
void bV(ab& world, const cA& frame, const ai& system);
double bM(
const ab& world,
const std::vector<int>& rids,
const ah& config);
ad bx(
const ab& world,
const ad& bL,
const ah& config);
std::vector<ad> bW(
const ab& world, int ac, const ah& config);
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>
namespace aC {
inline std::optional<int> findFirstRequest(
const ab& world, aa state, std::optional<int> remote = std::nullopt) {
for (std::size_t i = 0; i < world.aw.size(); ++i) {
const cw& request = world.aw[i];
if (request.state == state && (!remote.has_value() || request.remote == remote)) {
return static_cast<int>(i);
}
}
return std::nullopt;
}
inline std::vector<int> findRequests(
const ab& world, aa state, std::optional<int> remote, std::size_t limit) {
std::vector<int> rids;
rids.reserve(std::min(limit, world.aw.size()));
for (std::size_t i = 0; i < world.aw.size() && rids.size() < limit; ++i) {
const cw& request = world.aw[i];
if (request.state == state && (!remote.has_value() || request.remote == remote)) {
rids.push_back(static_cast<int>(i));
}
}
return rids;
}
template <typename aX>
std::vector<int> chooseDecodeRids(
const ab& world,
aa state,
std::optional<int> remote,
const aX& selector) {
return selector.select(world, state, remote);
}
inline std::optional<ad> choosePrefillPostAssignment(const ab& world) {
const auto rid = findFirstRequest(world, aa::cH);
if (!rid.has_value()) {
return std::nullopt;
}
const cw& request = world.aw.at(static_cast<std::size_t>(*rid));
assert(request.remote.has_value());
return ad{bt{bl::Edge, -1}, aZ{*request.remote, *rid}};
}
template <typename RemoteSelector>
std::optional<ad> choosePrefillPreAssignment(
const ab& world, const RemoteSelector& ak) {
const auto rid = findFirstRequest(world, aa::cE);
if (!rid.has_value()) {
return std::nullopt;
}
assert(!world.clouds.empty());
const int remote = ak(world, *rid);
assert(remote >= 0);
assert(static_cast<std::size_t>(remote) < world.clouds.size());
return ad{bt{bl::Edge, -1}, aH{remote, *rid}};
}
template <typename aX>
std::optional<ad> chooseDecodePreAssignment(
const ab& world, const aX& selector) {
std::vector<int> rids = chooseDecodeRids(
world, aa::ay, std::nullopt, selector);
if (rids.empty()) {
return std::nullopt;
}
return ad{bt{bl::Edge, -1}, aM{std::move(rids)}};
}
template <typename aX, typename RemoteSelector>
std::optional<ad> chooseEdgeAssignment(
const ab& world, const aX& selector, const RemoteSelector& ak) {
if (world.edge.busy) {
return std::nullopt;
}
if (std::vector<int> rids = chooseDecodeRids(
world, aa::cN, std::nullopt, selector); !rids.empty()) {
return ad{bt{bl::Edge, -1}, bf{std::move(rids)}};
}
if (selector.aP(world)) {
if (const auto ae = choosePrefillPostAssignment(world)) {
return ae;
}
if (selector.aU(world)) {
if (const auto ae = choosePrefillPreAssignment(world, ak)) {
return ae;
}
}
if (const auto ae = chooseDecodePreAssignment(world, selector)) {
return ae;
}
} else {
if (const auto ae = chooseDecodePreAssignment(world, selector)) {
return ae;
}
if (const auto ae = choosePrefillPostAssignment(world)) {
return ae;
}
}
if (const auto ae = choosePrefillPreAssignment(world, ak)) {
return ae;
}
return std::nullopt;
}
template <typename aX>
std::optional<ad> chooseCloudAssignment(
const ab& world, int remote, int ac, const aX& selector) {
const cx& cloud = world.clouds.at(static_cast<std::size_t>(remote));
if (cloud.busy) {
return std::nullopt;
}
const auto choose_prefill = [&]() -> std::optional<ad> {
const auto rid = findFirstRequest(world, aa::bE, remote);
if (!rid.has_value()) {
return std::nullopt;
}
const cw& request = world.aw.at(static_cast<std::size_t>(*rid));
assert(request.aN < ac);
return ad{
bt{bl::Cloud, remote},
aL{request.aN, ac, remote, *rid},
};
};
if (selector.aO(world, remote)) {
if (const auto ae = choose_prefill()) {
return ae;
}
}
if (std::vector<int> rids = chooseDecodeRids(
world, aa::bn, remote, selector); !rids.empty()) {
return ad{bt{bl::Cloud, remote}, be{remote, std::move(rids)}};
}
if (const auto ae = choose_prefill()) {
return ae;
}
return std::nullopt;
}
template <typename aX, typename RemoteSelector>
std::vector<ad> aR(
const ab& world,
int ac,
const aX& selector,
const RemoteSelector& ak) {
assert(ac > 0);
std::vector<ad> bm;
bm.reserve(world.clouds.size() + 1);
if (const auto edge_assignment = chooseEdgeAssignment(world, selector, ak)) {
bm.push_back(*edge_assignment);
}
for (std::size_t remote = 0; remote < world.clouds.size(); ++remote) {
if (const auto ae = chooseCloudAssignment(world, static_cast<int>(remote), ac, selector)) {
bm.push_back(*ae);
}
}
return bm;
}
struct RoundRobinRemoteSelector {
int operator()(const ab& world, int rid) const {
assert(!world.clouds.empty());
return rid % static_cast<int>(world.clouds.size());
}
};
template <typename aX>
std::vector<ad> aR(
const ab& world, int ac, const aX& selector) {
return aR(world, ac, selector, RoundRobinRemoteSelector{});
}
}
#include <iostream>
#include <string>
#include <vector>
#include <variant>
#include <cassert>
#include <stdexcept>
std::vector<int> readRequestIds(std::istream& input) {
int count;
input >> count;
assert(count >= 1);
std::vector<int> rids(static_cast<std::size_t>(count));
for (int& rid : rids) {
input >> rid;
}
return rids;
}
bt readServer(std::istream& input) {
std::string server_token;
input >> server_token;
if (server_token == "E") {
return {bl::Edge, -1};
}
if (server_token.size() >= 2 && server_token.front() == 'C') {
const int dl = std::stoi(server_token.substr(1));
return {bl::Cloud, dl};
}
throw std::runtime_error("Unknown server");
}
cy readTaskSpec(std::istream& input) {
int marker;
std::string family, stage;
input >> family >> stage;
if (family == "P") {
if (stage == "PRE") {
aH task{};
input >> task.remote >> task.rid;
return task;
}
if (stage == "PROC") {
aL task{};
input
>> task.dj
>> task.dk
>> task.remote
>> task.rid;
return task;
}
if (stage == "POST") {
aZ task{};
input >> task.remote >> task.rid;
return task;
}
}
if (family == "D") {
if (stage == "PRE") {
aM task{};
input >> marker;
assert(marker == -1);
task.rids = readRequestIds(input);
return task;
}
if (stage == "PROC") {
be task{};
input >> task.remote;
task.rids = readRequestIds(input);
return task;
}
if (stage == "POST") {
bf task{};
input >> marker;
assert(marker == -1);
task.rids = readRequestIds(input);
return task;
}
}
throw std::runtime_error("Unknown task spec");
}
af parseTransferDirection(const std::string& token) {
if (token == "UP") return af::Up;
if (token == "DOWN") return af::Down;
throw std::runtime_error("Unknown transfer direction");
}
aY parseTransferStage(const std::string& token) {
if (token == "PRE") return aY::cQ;
if (token == "DEC") return aY::Decode;
throw std::runtime_error("Unknown transfer stage");
}
cz readEvent(std::istream& input) {
std::string type;
input >> type;
if (type == "ARR") {
cB event{};
input >> event.rid >> event.bg;
return event;
}
if (type == "TDN") {
cC event{};
event.server = readServer(input);
event.task = readTaskSpec(input);
input >> event.duration;
return event;
}
if (type == "XDN") {
aS event{};
std::string direction_token;
std::string stage_token;
input
>> direction_token
>> event.remote
>> event.size_bytes
>> stage_token;
event.au = parseTransferDirection(direction_token);
event.stage = parseTransferStage(stage_token);
event.rids = readRequestIds(input);
return event;
}
if (type == "FIN") {
cD event{};
input >> event.rid;
return event;
}
throw std::runtime_error("Unknown event type");
}
std::optional<cA> db(std::istream& input) {
std::string timestamp_token;
if (!(input >> timestamp_token) || timestamp_token == "END") {
return std::nullopt;
}
cA frame{};
frame.timestamp = std::stod(timestamp_token);
int event_count;
input >> event_count;
assert(event_count >= 0);
frame.events.reserve(static_cast<std::size_t>(event_count));
for (int i = 0; i < event_count; ++i) {
frame.events.push_back(readEvent(input));
}
return frame;
}
namespace {
template <typename... Visitors>
struct OutputVisitor : Visitors... {
using Visitors::operator()...;
};
template <typename... Visitors>
OutputVisitor(Visitors...) -> OutputVisitor<Visitors...>;
void writeRequestIds(std::ostream& output, const std::vector<int>& rids) {
output << rids.size();
for (const int rid : rids) {
output << ' ' << rid;
}
}
void writeServer(std::ostream& output, const bt& server) {
if (server.type == bl::Edge) {
output << "E";
} else {
output << "C" << server.dl;
}
}
void writeTaskSpec(std::ostream& output, const cy& task) {
std::visit(OutputVisitor{
[&](const aH& value) {
output << "P PRE " << value.remote << ' ' << value.rid;
},
[&](const aL& value) {
output << "P PROC " << value.dj << ' ' << value.dk << ' ' << value.remote << ' ' << value.rid;
},
[&](const aZ& value) {
output << "P POST " << value.remote << ' ' << value.rid;
},
[&](const aM& value) {
output << "D PRE -1 ";
writeRequestIds(output, value.rids);
},
[&](const be& value) {
output << "D PROC " << value.remote << ' ';
writeRequestIds(output, value.rids);
},
[&](const bf& value) {
output << "D POST -1 ";
writeRequestIds(output, value.rids);
},
}, task);
}
}
void de(std::ostream& output, const std::vector<ad>& bm) {
output << bm.size() << '\n';
for (const ad& ae : bm) {
writeServer(output, ae.server);
output << ' ';
writeTaskSpec(output, ae.task);
output << '\n';
}
}
#include <istream>
#include <cassert>
double transferTime(const ai& config, std::int64_t len) {
assert(config.dh > 0);
const std::int64_t data_bytes = len * config.dg;
return config.dh + 8.0 * static_cast<double>(data_bytes)
/ (config.di * 1'000'000.0);
}
ai cY(std::istream& input) {
ai config{};
input >> config.K
>> config.S
>> config.dh
>> config.di
>> config.dg
>> config.ac
>> config.SLO1
>> config.SLO2
>> config.tp_UB
>> config.tp_base
>> config.dist_base
>> config.w_tp
>> config.w_c;
return config;
}
#include <istream>
#include <vector>
#include <cassert>
#include <algorithm>
#include <iterator>
double aF(const bk& curve, int size) {
assert(!curve.points.empty());
if (size <= curve.points.front().av) {
return curve.points.front().duration;
}
if (curve.points.back().av <= size) {
return curve.points.back().duration;
}
const auto right = std::lower_bound(
curve.points.begin(), curve.points.end(), size, [](const TimingPoint& point, int target) {
return point.av < target;
});
assert(right != curve.points.begin());
assert(right != curve.points.end());
if (right->av == size) {
return right->duration;
}
const TimingPoint& left = *std::prev(right);
return left.duration
+ (size - left.av) * (right->duration - left.duration)
/ (right->av - left.av);
}
namespace {
using DurationMember = double bC::*;
void finalizeCurve(bk& curve) {
assert(!curve.points.empty());
std::sort(curve.points.begin(), curve.points.end(), [](const TimingPoint& a, const TimingPoint& b) {
return a.av < b.av;
});
}
bk makeTimingCurve(const bd& table, DurationMember member) {
bk curve;
curve.points.reserve(table.rows.size());
for (const bC& row : table.rows) {
const double duration = row.*member;
if (duration != -1.0) {
curve.points.push_back({row.av, duration});
}
}
finalizeCurve(curve);
return curve;
}
}
ax da(const bd& table) {
return {
.cT  = makeTimingCurve(table, &bC::cT),
.cU = makeTimingCurve(table, &bC::cU),
.cV = makeTimingCurve(table, &bC::cV),
.cW   = makeTimingCurve(table, &bC::cW),
.bB  = makeTimingCurve(table, &bC::bB),
.cX  = makeTimingCurve(table, &bC::cX),
};
}
bd cZ(std::istream& input) {
int n;
input >> n;
bd table;
table.rows.reserve(n);
for (int i = 0; i < n; i++) {
bC row{};
input >> row.av;
input >> row.cT >> row.cU >> row.cV;
input >> row.cW >> row.bB >> row.cX;
table.rows.push_back(row);
}
return table;
}
#include <cassert>
#include <variant>
namespace {
template <typename... Visitors>
struct Overloaded : Visitors... {
using Visitors::operator()...;
};
template <typename... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;
cw& ap(ab& world, int rid) {
assert(rid >= 0);
const auto index = static_cast<std::size_t>(rid);
assert(index < world.aw.size());
return world.aw.at(index);
}
cx& getServer(ab& world, const bt& server) {
if (server.type == bl::Edge) {
return world.edge;
}
assert(server.dl >= 0);
const auto index = static_cast<std::size_t>(server.dl);
assert(index < world.clouds.size());
return world.clouds.at(index);
}
void bj([[maybe_unused]] const bt& server) {
assert(server.type == bl::Edge);
}
void assertCloudServer([[maybe_unused]] const bt& server, [[maybe_unused]] int remote) {
assert(server.type == bl::Cloud);
assert(server.dl == remote);
}
void aK([[maybe_unused]] const cw& request, [[maybe_unused]] int remote) {
assert(request.remote.has_value());
assert(*request.remote == remote);
}
void applyArrival(ab& world, const cB& arrival) {
assert(arrival.rid >= 0);
assert(arrival.bg > 0);
[[maybe_unused]] const auto rid = static_cast<std::size_t>(arrival.rid);
assert(rid == world.aw.size());
world.aw.push_back({
.bg = arrival.bg,
.by = world.aG,
.remote = std::nullopt,
.aN = 0,
.dm = 0,
.aA = std::nullopt,
.state = aa::cE,
});
}
void applyTaskDone(ab& world, const cC& event, int ac) {
assert(ac > 0);
cx& server = getServer(world, event.server);
assert(server.busy);
std::visit(Overloaded{
[&](const aH& task) {
bj(event.server);
cw& request = ap(world, task.rid);
assert(request.state == aa::bA);
aK(request, task.remote);
request.state = aa::cF;
},
[&](const aL& task) {
assertCloudServer(event.server, task.remote);
assert(task.dj >= 0);
assert(task.dj < task.dk);
assert(task.dk <= ac);
cw& request = ap(world, task.rid);
assert(request.state == aa::bz);
aK(request, task.remote);
assert(request.aN == task.dj);
request.aN = task.dk;
request.state = task.dk == ac
? aa::cG
: aa::bE;
},
[&](const aZ& task) {
bj(event.server);
cw& request = ap(world, task.rid);
assert(request.state == aa::cI);
aK(request, task.remote);
world.dn += world.aG - request.by;
++world.dp;
request.state = aa::ay;
},
[&](const aM& task) {
bj(event.server);
assert(!task.rids.empty());
for (const int rid : task.rids) {
cw& request = ap(world, rid);
assert(request.state == aa::cJ);
assert(request.remote.has_value());
request.state = aa::cK;
}
},
[&](const be& task) {
assertCloudServer(event.server, task.remote);
assert(!task.rids.empty());
for (const int rid : task.rids) {
cw& request = ap(world, rid);
assert(request.state == aa::cL);
aK(request, task.remote);
request.state = aa::cM;
}
},
[&](const bf& task) {
bj(event.server);
assert(!task.rids.empty());
for (const int rid : task.rids) {
cw& request = ap(world, rid);
assert(request.state == aa::cO);
if (request.aA.has_value()) {
world.dq += world.aG - *request.aA;
++world.dr;
}
++request.dm;
request.aA = world.aG;
request.state = aa::ay;
}
},
}, event.task);
server.busy = false;
}
void applyTransferDone(ab& world, const aS& event) {
assert(event.remote >= 0);
assert(static_cast<std::size_t>(event.remote) < world.clouds.size());
assert(!event.rids.empty());
if (event.stage == aY::cQ) {
assert(event.rids.size() == 1);
cw& request = ap(world, event.rids.front());
aK(request, event.remote);
if (event.au == af::Up) {
assert(request.state == aa::cF);
request.state = aa::bE;
} else {
assert(request.state == aa::cG);
request.state = aa::cH;
}
return;
}
for (const int rid : event.rids) {
cw& request = ap(world, rid);
aK(request, event.remote);
if (event.au == af::Up) {
assert(request.state == aa::cK);
request.state = aa::bn;
} else {
assert(request.state == aa::cM);
request.state = aa::cN;
}
}
}
void applyFinish(ab& world, const cD& finish) {
cw& request = ap(world, finish.rid);
assert(request.state == aa::ay);
assert(request.dm > 0);
request.state = aa::cP;
}
}
void dc(ab& world, const cA& frame, int ac) {
assert(frame.timestamp >= world.aG);
world.aG = frame.timestamp;
for (const cz& event : frame.events) {
if (const cB* arrival = std::get_if<cB>(&event)) {
applyArrival(world, *arrival);
} else if (const cC* task_done = std::get_if<cC>(&event)) {
applyTaskDone(world, *task_done, ac);
} else if (const aS* transfer_done = std::get_if<aS>(&event)) {
applyTransferDone(world, *transfer_done);
}
}
for (const cz& event : frame.events) {
if (const cD* finish = std::get_if<cD>(&event)) {
applyFinish(world, *finish);
}
}
}
void dd(ab& world, const ad& ae, [[maybe_unused]] int ac) {
assert(ac > 0);
cx& server = getServer(world, ae.server);
assert(!server.busy);
std::visit(Overloaded{
[&](const aH& task) {
bj(ae.server);
cw& request = ap(world, task.rid);
assert(request.state == aa::cE);
assert(!request.remote.has_value());
assert(task.remote >= 0);
assert(static_cast<std::size_t>(task.remote) < world.clouds.size());
request.remote = task.remote;
request.state = aa::bA;
},
[&](const aL& task) {
assertCloudServer(ae.server, task.remote);
cw& request = ap(world, task.rid);
assert(request.state == aa::bE);
aK(request, task.remote);
assert(task.dj == request.aN);
assert(task.dj >= 0);
assert(task.dj < task.dk);
assert(task.dk <= ac);
request.state = aa::bz;
},
[&](const aZ& task) {
bj(ae.server);
cw& request = ap(world, task.rid);
assert(request.state == aa::cH);
aK(request, task.remote);
request.state = aa::cI;
},
[&](const aM& task) {
bj(ae.server);
assert(!task.rids.empty());
for (const int rid : task.rids) {
[[maybe_unused]] const cw& request = ap(world, rid);
assert(request.state == aa::ay);
assert(request.remote.has_value());
}
for (const int rid : task.rids) {
ap(world, rid).state = aa::cJ;
}
},
[&](const be& task) {
assertCloudServer(ae.server, task.remote);
assert(!task.rids.empty());
for (const int rid : task.rids) {
[[maybe_unused]] const cw& request = ap(world, rid);
assert(request.state == aa::bn);
aK(request, task.remote);
}
for (const int rid : task.rids) {
ap(world, rid).state = aa::cL;
}
},
[&](const bf& task) {
bj(ae.server);
assert(!task.rids.empty());
for (const int rid : task.rids) {
[[maybe_unused]] const cw& request = ap(world, rid);
assert(request.state == aa::cN);
assert(request.remote.has_value());
}
for (const int rid : task.rids) {
ap(world, rid).state = aa::cO;
}
},
}, ae.task);
server.busy = true;
}
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>
#include <vector>
namespace {
std::vector<int> buildBatchChoices(const bk& curve, double aE, int max_batch_size) {
assert(aE >= 0.0);
assert(max_batch_size >= 1);
std::vector<int> choices(static_cast<std::size_t>(max_batch_size + 1));
int best_batch_size = 1;
double best_cost = aE + aF(curve, 1);
choices[1] = 1;
for (int av = 2; av <= max_batch_size; ++av) {
const double cost = (aE + aF(curve, av)) / av;
if (cost < best_cost) {
best_batch_size = av;
best_cost = cost;
}
choices[static_cast<std::size_t>(av)] = best_batch_size;
}
return choices;
}
class BatchedDecodeSelector {
public:
explicit BatchedDecodeSelector(const aq& policy)
: policy_(policy) {}
std::vector<int> select(
const ab& world, aa state, std::optional<int> remote) const {
std::vector<int> rids = aC::findRequests(
world, state, remote, std::numeric_limits<std::size_t>::max());
if (!rids.empty()) {
rids.resize(static_cast<std::size_t>(aI(policy_, state, rids.size())));
}
return rids;
}
bool aP(const ab&) const {
return true;
}
bool aU(const ab&) const {
return false;
}
bool aO(const ab&, int) const {
return false;
}
private:
const aq& policy_;
};
}
int aI(const aq& policy, aa state, std::size_t ready_count) {
assert(ready_count > 0);
const std::vector<int>* choices = nullptr;
if (state == aa::ay) {
choices = &policy.pre_by_ready_count;
} else if (state == aa::bn) {
choices = &policy.proc_by_ready_count;
} else {
assert(state == aa::cN);
choices = &policy.post_by_ready_count;
}
assert(choices->size() > 1);
const std::size_t index = std::min(ready_count, choices->size() - 1);
return choices->at(index);
}
aq buildDecodeBatchPolicy(const ax& curves, double aE, int max_batch_size) {
return {
.pre_by_ready_count = buildBatchChoices(curves.cW, aE, max_batch_size),
.proc_by_ready_count = buildBatchChoices(curves.bB, aE, max_batch_size),
.post_by_ready_count = buildBatchChoices(curves.cX, aE, max_batch_size),
};
}
std::vector<ad> bc(
const ab& world, int ac, const aq& batch_policy) {
return aC::aR(world, ac, BatchedDecodeSelector{batch_policy});
}
std::vector<ad> bc(
const ab& world,
int ac,
const aq& batch_policy,
const aJ& ak) {
return aC::aR(
world, ac, BatchedDecodeSelector{batch_policy}, ak);
}
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>
#include <vector>
namespace {
bool isPrefillState(aa state) {
switch (state) {
case aa::cE:
case aa::bA:
case aa::cF:
case aa::bE:
case aa::bz:
case aa::cG:
case aa::cH:
case aa::cI:
return true;
default:
return false;
}
}
bool isColdAdmissionInFlight(const cw& request) {
return aD(request) == aj::cR
&& request.state != aa::ay;
}
double normalizedExcess(double elapsed, double target) {
assert(target > 0.0);
return std::max(0.0, elapsed / target - 1.0);
}
double hotUrgency(const ab& world, const cw& request, double slo_tpot) {
assert(aD(request) == aj::cS);
assert(request.aA.has_value());
const double aA = request.aA.value_or(request.by);
return (world.aG - aA) / slo_tpot;
}
double coldUrgency(const ab& world, const cw& request, double slo_tdr) {
return (world.aG - request.by) / slo_tdr;
}
struct Candidate {
int rid;
aj am;
double urgency;
};
class ScoreAwareDecodeSelector {
public:
explicit ScoreAwareDecodeSelector(const an& config)
: config_(config) {}
std::vector<int> select(
const ab& world, aa state, std::optional<int> remote) const {
if (state == aa::ay && shouldWarmUpPrefills(world)) {
return {};
}
const std::vector<int> ready = aC::findRequests(
world, state, remote, std::numeric_limits<std::size_t>::max());
std::vector<Candidate> candidates;
candidates.reserve(ready.size());
for (const int rid : ready) {
const cw& request = world.aw.at(static_cast<std::size_t>(rid));
const aj am = aD(request);
assert(am == aj::cR || am == aj::cS);
const double urgency = am == aj::cS
? hotUrgency(world, request, config_.slo_tpot)
: coldUrgency(world, request, config_.slo_tdr);
candidates.push_back({rid, am, urgency});
}
std::sort(candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
if (left.am != right.am) {
return left.am == aj::cS;
}
if (left.urgency != right.urgency) {
return left.urgency > right.urgency;
}
return left.rid < right.rid;
});
int remaining_cold_admissions = state == aa::ay
? std::max(0, config_.ao - countActiveHotSet(world))
: std::numeric_limits<int>::max();
std::vector<int> selected;
selected.reserve(candidates.size());
for (const Candidate& candidate : candidates) {
if (state == aa::ay && candidate.am == aj::cR) {
if (remaining_cold_admissions == 0) {
continue;
}
--remaining_cold_admissions;
}
selected.push_back(candidate.rid);
}
if (!selected.empty()) {
selected.resize(static_cast<std::size_t>(
aI(config_.bJ, state, selected.size())));
}
return selected;
}
bool aP(const ab& world) const {
double tdr_pressure = 0.0;
double tpot_pressure = 0.0;
for (const cw& request : world.aw) {
if (request.state == aa::cH) {
tdr_pressure = std::max(
tdr_pressure,
normalizedExcess(world.aG - request.by, config_.slo_tdr));
}
if (request.state == aa::ay && aD(request) == aj::cS) {
tpot_pressure = std::max(
tpot_pressure,
normalizedExcess(
world.aG - request.aA.value_or(request.by),
config_.slo_tpot));
}
}
if (config_.waiting_weight > 0.0 && (tdr_pressure > 0.0 || tpot_pressure > 0.0)) {
return tdr_pressure > 0.0 && tdr_pressure >= tpot_pressure;
}
return countActiveHotSet(world) < config_.ao;
}
bool aU(const ab& world) const {
return shouldWarmUpPrefills(world);
}
bool aO(const ab& world, int) const {
return shouldWarmUpPrefills(world);
}
private:
bool shouldWarmUpPrefills(const ab& world) const {
return config_.prefill_warmup
&& countActiveHotSet(world) == 0
&& std::any_of(world.aw.begin(), world.aw.end(), [](const cw& request) {
return aD(request) == aj::cQ;
});
}
const an& config_;
};
}
aj aD(const cw& request) {
if (request.state == aa::cP) {
return aj::cP;
}
if (isPrefillState(request.state)) {
return aj::cQ;
}
return request.dm == 0 ? aj::cR : aj::cS;
}
int countActiveHotSet(const ab& world) {
return static_cast<int>(std::count_if(
world.aw.begin(), world.aw.end(), [](const cw& request) {
return aD(request) == aj::cS || isColdAdmissionInFlight(request);
}));
}
an buildScoreAwareSchedulerConfig(
const ai& system,
const ax& curves,
std::optional<int> ao) {
aq bJ = buildDecodeBatchPolicy(curves, system.S);
const double singleton_decode_cycle =
3.0 * system.S
+ aF(curves.cW, 1)
+ aF(curves.bB, 1)
+ aF(curves.cX, 1)
+ 2.0 * transferTime(system, 1);
const double tpot_slack = std::clamp(system.SLO2 / singleton_decode_cycle, 0.5, 4.0);
const double concurrency_per_cloud = (4.0 + 28.0 * system.w_tp) * tpot_slack;
const int horizon = std::clamp(
static_cast<int>(std::ceil(system.K * concurrency_per_cloud)), 1, 4096);
const int curve_target = std::max({
aI(bJ, aa::ay, horizon),
aI(bJ, aa::bn, horizon),
aI(bJ, aa::cN, horizon),
});
const int derived_target = std::max(2 * system.K, curve_target);
const int target = ao.value_or(derived_target);
assert(target >= 1);
return {
.bJ = std::move(bJ),
.ao = target,
.slo_tdr = system.SLO1,
.slo_tpot = system.SLO2,
.waiting_weight = system.w_c,
.prefill_warmup = false,
};
}
std::vector<ad> aQ(
const ab& world, int ac, const an& config) {
assert(config.ao >= 1);
assert(config.slo_tdr > 0.0);
assert(config.slo_tpot > 0.0);
return aC::aR(world, ac, ScoreAwareDecodeSelector{config});
}
std::vector<ad> aQ(
const ab& world,
int ac,
const an& config,
const aJ& ak) {
assert(config.ao >= 1);
assert(config.slo_tdr > 0.0);
assert(config.slo_tpot > 0.0);
return aC::aR(
world, ac, ScoreAwareDecodeSelector{config}, ak);
}
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>
#include <vector>
namespace {
constexpr int max_request_count = 2000;
constexpr int pipeline_batch_count = 4;
bool hasPrefillWork(const ab& world) {
return std::any_of(world.aw.begin(), world.aw.end(), [](const cw& request) {
return aD(request) == aj::cQ;
});
}
int countDecodePopulation(const ab& world) {
return static_cast<int>(std::count_if(
world.aw.begin(), world.aw.end(), [](const cw& request) {
const aj am = aD(request);
return am == aj::cR || am == aj::cS;
}));
}
bool hasPendingDecodeUpload(const ab& world, std::optional<int> remote) {
return std::any_of(world.aw.begin(), world.aw.end(), [&](const cw& request) {
const bool matches_remote = !remote.has_value() || request.remote == remote;
return matches_remote
&& (request.state == aa::cJ
|| request.state == aa::cK);
});
}
bool hasPendingDecodeDownload(const ab& world) {
return std::any_of(world.aw.begin(), world.aw.end(), [](const cw& request) {
return request.state == aa::bn
|| request.state == aa::cL
|| request.state == aa::cM;
});
}
bool waitingSlosComfortablyMet(
const ab& world, const an& aV) {
if (world.dp == 0 || world.dr == 0) {
return false;
}
const double observed_tdr = world.dn / world.dp;
const double observed_tpot = world.dq / world.dr;
return observed_tdr <= 0.8 * aV.slo_tdr
&& observed_tpot <= 0.8 * aV.slo_tpot;
}
class ThroughputDecodeSelector {
public:
explicit ThroughputDecodeSelector(const as& config)
: config_(config) {}
std::vector<int> select(
const ab& world, aa state, std::optional<int> remote) const {
std::vector<int> ready = aC::findRequests(
world, state, remote, std::numeric_limits<std::size_t>::max());
if (ready.empty() || shouldWaitForBatch(world, state, remote, ready.size())) {
return {};
}
if (state == aa::ay) {
const int available_admissions =
std::max(0, config_.ao - countActiveHotSet(world));
int remaining_admissions = available_admissions;
ready.erase(std::remove_if(ready.begin(), ready.end(), [&](int rid) {
const cw& request = world.aw.at(static_cast<std::size_t>(rid));
if (aD(request) != aj::cR) {
return false;
}
if (remaining_admissions == 0) {
return true;
}
--remaining_admissions;
return false;
}), ready.end());
}
if (!ready.empty()) {
ready.resize(static_cast<std::size_t>(
aI(config_.throughput_decode_batches, state, ready.size())));
}
return ready;
}
bool aP(const ab&) const {
return true;
}
bool aU(const ab& world) const {
return countDecodePopulation(world) < config_.ao;
}
bool aO(const ab& world, int) const {
return countDecodePopulation(world) < config_.ao;
}
private:
bool shouldWaitForBatch(
const ab& world,
aa state,
std::optional<int> remote,
std::size_t ready_count) const {
const int preferred_size = state == aa::bn
? config_.bG
: config_.preferred_decode_batch_size;
if (ready_count >= static_cast<std::size_t>(preferred_size)) {
return false;
}
if (state == aa::ay) {
return countDecodePopulation(world) < config_.ao && hasPrefillWork(world);
}
if (state == aa::bn) {
return hasPendingDecodeUpload(world, remote);
}
assert(state == aa::cN);
return hasPendingDecodeDownload(world);
}
const as& config_;
};
std::vector<ad> chooseAdaptiveAssignmentsImpl(
const ab& world,
int ac,
const as& config,
const aJ* ak) {
const auto choose_batched = [&] {
return ak == nullptr
? bc(world, ac, config.bv)
: bc(world, ac, config.bv, *ak);
};
const auto choose_score_aware = [&] {
return ak == nullptr
? aQ(world, ac, config.aV)
: aQ(world, ac, config.aV, *ak);
};
if (config.regime == ar::Balanced) {
return choose_batched();
}
if (config.regime == ar::HardSlo) {
return choose_score_aware();
}
if (config.regime == ar::Waiting) {
return waitingSlosComfortablyMet(world, config.aV)
? choose_batched()
: choose_score_aware();
}
const ThroughputDecodeSelector decode_selector{config};
return ak == nullptr
? aC::aR(world, ac, decode_selector)
: aC::aR(world, ac, decode_selector, *ak);
}
}
ar classifyScoreRegime(const ai& system) {
assert(system.w_tp >= 0.0);
assert(system.w_c >= 0.0);
if (system.dist_base == 0.0 && system.w_c >= 0.1) {
return ar::HardSlo;
}
if (system.w_tp >= 0.8) {
return ar::Throughput;
}
if (system.w_c >= 0.7) {
return ar::Waiting;
}
return ar::Balanced;
}
double estimateDecodeCapacity(
const ai& system, const ax& curves, int bh) {
assert(bh >= 1);
const int active_clouds = std::min(system.K, bh);
const int bD = (bh + active_clouds - 1) / active_clouds;
const double edge_service_time =
2.0 * system.S
+ aF(curves.cW, bh)
+ aF(curves.cX, bh);
const double cloud_service_time = system.S + aF(curves.bB, bD);
const double serialized_bytes_time =
8.0 * static_cast<double>(bh) * system.dg
/ (system.di * 1'000'000.0);
const double link_service_time = active_clouds * system.dh + serialized_bytes_time;
const double bottleneck_time = std::max({edge_service_time, cloud_service_time, link_service_time});
return bh / bottleneck_time;
}
as buildAdaptiveSchedulerConfig(
const ai& system, const ax& curves) {
aq bL = buildDecodeBatchPolicy(curves, system.S);
const ar regime = classifyScoreRegime(system);
const std::optional<int> waiting_target = regime == ar::HardSlo
? std::optional<int>{system.K}
: std::nullopt;
an aV =
buildScoreAwareSchedulerConfig(system, curves, waiting_target);
aV.prefill_warmup =
regime == ar::HardSlo && system.w_tp <= 0.1;
const double safety_margin = 1.05 + 0.1 * system.w_tp;
const double throughput_target = system.tp_UB * safety_margin;
int aW = 1;
double bH = estimateDecodeCapacity(system, curves, 1);
bool reached_target = bH >= throughput_target;
for (int av = 2; av <= max_request_count; ++av) {
const double capacity = estimateDecodeCapacity(system, curves, av);
if (!reached_target && capacity > bH) {
aW = av;
bH = capacity;
}
if (!reached_target && capacity >= throughput_target) {
aW = av;
bH = capacity;
reached_target = true;
}
}
const int bD =
(aW + std::min(system.K, aW) - 1)
/ std::min(system.K, aW);
const int throughput_hot_target = std::min(
max_request_count,
std::max(2 * system.K, pipeline_batch_count * aW));
const int hot_target = regime == ar::Throughput
? throughput_hot_target
: aV.ao;
return {
.bv = bL,
.throughput_decode_batches = std::move(bL),
.aV = aV,
.regime = regime,
.preferred_decode_batch_size = aW,
.bG = bD,
.ao = hot_target,
.estimated_decode_capacity = bH,
.throughput_target = throughput_target,
};
}
std::vector<ad> bo(
const ab& world, int ac, const as& config) {
return chooseAdaptiveAssignmentsImpl(world, ac, config, nullptr);
}
std::vector<ad> bo(
const ab& world,
int ac,
const as& config,
const aJ& ak) {
return chooseAdaptiveAssignmentsImpl(world, ac, config, &ak);
}
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>
namespace {
bool hasOutstandingPrefillCompute(aa state) {
switch (state) {
case aa::bA:
case aa::cF:
case aa::bE:
case aa::bz:
return true;
default:
return false;
}
}
}
al bK(
const ai& system, const ax& curves) {
const as adaptive = buildAdaptiveSchedulerConfig(system, curves);
const int bD = std::max(1, adaptive.bG);
const double decode_batch_work = system.S + aF(curves.bB, bD);
return {
.adaptive = adaptive,
.prefill_proc_curve = curves.cU,
.aE = system.S,
.bu = decode_batch_work / bD,
.ac = system.ac,
};
}
std::vector<double> estimateCloudWorkloads(
const ab& world, const al& config) {
assert(config.ac > 0);
assert(config.aE >= 0.0);
assert(config.bu > 0.0);
std::vector<double> workloads(world.clouds.size(), 0.0);
for (const cw& request : world.aw) {
if (!request.remote.has_value() || request.state == aa::cP) {
continue;
}
const std::size_t remote = static_cast<std::size_t>(*request.remote);
assert(remote < workloads.size());
workloads[remote] += config.bu;
if (hasOutstandingPrefillCompute(request.state)) {
assert(request.aN >= 0);
assert(request.aN < config.ac);
const double remaining_fraction = static_cast<double>(config.ac - request.aN)
/ config.ac;
workloads[remote] += config.aE
+ remaining_fraction * aF(config.prefill_proc_curve, request.bg);
}
}
return workloads;
}
std::vector<double> estimatePlacementScores(
const ab& world, int rid, const al& config) {
assert(rid >= 0);
assert(static_cast<std::size_t>(rid) < world.aw.size());
const cw& candidate = world.aw[static_cast<std::size_t>(rid)];
assert(candidate.state == aa::cE);
assert(!candidate.remote.has_value());
std::vector<double> scores = estimateCloudWorkloads(world, config);
const double candidate_work = config.aE
+ aF(config.prefill_proc_curve, candidate.bg)
+ config.bu;
for (double& score : scores) {
score += candidate_work;
}
return scores;
}
int chooseLoadAwareRemote(
const ab& world, int rid, const al& config) {
assert(!world.clouds.empty());
const std::vector<double> scores = estimatePlacementScores(world, rid, config);
int best_remote = rid % static_cast<int>(scores.size());
for (std::size_t remote = 0; remote < scores.size(); ++remote) {
if (scores[remote] < scores[static_cast<std::size_t>(best_remote)]) {
best_remote = static_cast<int>(remote);
}
}
return best_remote;
}
std::vector<ad> chooseMultiprocessorAssignments(
const ab& world, int ac, const al& config) {
assert(ac == config.ac);
const aJ ak = [&](const ab& current, int rid) {
return chooseLoadAwareRemote(current, rid, config);
};
return bo(world, ac, config.adaptive, ak);
}
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
namespace {
template <typename... Visitors>
struct bS : Visitors... {
using Visitors::operator()...;
};
template <typename... Visitors>
bS(Visitors...) -> bS<Visitors...>;
bool sameServer(const bt& left, const bt& right) {
return left.type == right.type && left.dl == right.dl;
}
bool sameTask(const cy& left, const cy& right) {
if (left.index() != right.index()) {
return false;
}
return std::visit(bS{
[](const aH& a, const aH& b) {
return a.remote == b.remote && a.rid == b.rid;
},
[](const aL& a, const aL& b) {
return a.dj == b.dj && a.dk == b.dk
&& a.remote == b.remote && a.rid == b.rid;
},
[](const aZ& a, const aZ& b) {
return a.remote == b.remote && a.rid == b.rid;
},
[](const aM& a, const aM& b) {
return a.rids == b.rids;
},
[](const be& a, const be& b) {
return a.remote == b.remote && a.rids == b.rids;
},
[](const bf& a, const bf& b) {
return a.rids == b.rids;
},
[](const auto&, const auto&) {
return false;
},
}, left, right);
}
const cw& ap(const ab& world, int rid) {
assert(rid >= 0);
return world.aw.at(static_cast<std::size_t>(rid));
}
br& directionState(bw& links, af au) {
return au == af::Up ? links.up : links.down;
}
const br& directionState(
const bw& links, af au) {
return au == af::Up ? links.up : links.down;
}
std::vector<ag> bp(
const ab& world, const cy& task, int ac) {
return std::visit(bS{
[&](const aH& value) {
const cw& request = ap(world, value.rid);
return std::vector<ag>{
{af::Up, value.remote, aY::cQ, request.bg, {value.rid}},
};
},
[&](const aL& value) {
if (value.dk != ac) {
return std::vector<ag>{};
}
const cw& request = ap(world, value.rid);
return std::vector<ag>{
{af::Down, value.remote, aY::cQ, request.bg, {value.rid}},
};
},
[](const aZ&) {
return std::vector<ag>{};
},
[&](const aM& value) {
std::vector<std::vector<int>> by_remote(world.clouds.size());
for (const int rid : value.rids) {
const cw& request = ap(world, rid);
assert(request.remote.has_value());
by_remote.at(static_cast<std::size_t>(*request.remote)).push_back(rid);
}
std::vector<ag> aB;
for (std::size_t remote = 0; remote < by_remote.size(); ++remote) {
if (!by_remote[remote].empty()) {
aB.push_back({
af::Up,
static_cast<int>(remote),
aY::Decode,
static_cast<std::int64_t>(by_remote[remote].size()),
std::move(by_remote[remote]),
});
}
}
return aB;
},
[](const be& value) {
return std::vector<ag>{
{af::Down, value.remote, aY::Decode,
static_cast<std::int64_t>(value.rids.size()), value.rids},
};
},
[](const bf&) {
return std::vector<ag>{};
},
}, task);
}
void commitTransfer(
bw& links,
const ag& az,
double queued_at,
const ai& system) {
br& au = directionState(links, az.au);
const double starts_at = std::max(queued_at, au.bN);
const double completes_at = starts_at + transferTime(system, az.length);
au.bN = completes_at;
au.committed.push_back({az, queued_at, starts_at, completes_at});
}
bool sameTransfer(const ag& expected, const aS& actual) {
return expected.au == actual.au
&& expected.remote == actual.remote
&& expected.stage == actual.stage
&& expected.rids == actual.rids;
}
void reconcileTransfer(
bw& links,
const aS& event,
double timestamp,
const ai& system) {
br& au = directionState(links, event.au);
assert(!au.committed.empty());
const bP& expected = au.committed.front();
assert(sameTransfer(expected.az, event));
assert(event.size_bytes == expected.az.length * system.dg);
const double error = std::abs(timestamp - expected.completes_at);
links.ds = std::max(links.ds, error);
++links.dt;
au.committed.pop_front();
}
struct bQ {
ag az;
double bb;
std::uint64_t trigger_order;
std::size_t dv;
};
std::vector<bQ> futureTransfers(
const bw& links, af au) {
std::vector<bQ> result;
for (const bO& trigger : links.bq) {
for (std::size_t index = 0; index < trigger.aB.size(); ++index) {
if (trigger.aB[index].au == au) {
result.push_back({trigger.aB[index], trigger.du, trigger.order, index});
}
}
}
std::sort(result.begin(), result.end(), [](const bQ& left, const bQ& right) {
return std::tie(left.bb, left.trigger_order, left.dv)
< std::tie(right.bb, right.trigger_order, right.dv);
});
return result;
}
double tailBefore(
const bw& links,
af au,
double bb,
const ai& system) {
double tail = directionState(links, au).bN;
for (const bQ& future : futureTransfers(links, au)) {
if (future.bb > bb) {
break;
}
tail = std::max(tail, future.bb) + transferTime(system, future.az.length);
}
return tail;
}
struct bR {
aj am;
double age;
int rid;
};
bR priorityOf(const ab& world, int rid) {
const cw& request = ap(world, rid);
const aj am = aD(request);
assert(am == aj::cR || am == aj::cS);
const double origin = am == aj::cS
? request.aA.value_or(request.by)
: request.by;
return {am, world.aG - origin, rid};
}
int classRank(aj am) {
return am == aj::cS ? 1 : 0;
}
bool cv(const bR& left, const bR& right) {
if (left.am != right.am) {
return classRank(left.am) > classRank(right.am);
}
if (left.age != right.age) {
return left.age > right.age;
}
return left.rid < right.rid;
}
bool cu(
const bR& candidate,
const bR& original,
const ah& config) {
if (classRank(candidate.am) > classRank(original.am)) {
return true;
}
if (candidate.am != original.am) {
return false;
}
const double permitted_age_loss = config.system.dh;
return original.age - candidate.age <= permitted_age_loss;
}
bool hasGuaranteedFutureEvent(const ab& world) {
if (world.edge.busy
|| std::any_of(world.clouds.begin(), world.clouds.end(), [](const cx& server) {
return server.busy;
})) {
return true;
}
return !world.links.up.committed.empty()
|| !world.links.down.committed.empty()
|| !world.links.bq.empty();
}
std::optional<ag> cg(
const ab& world, const ad& ae, int ac) {
const std::vector<ag> aB = bp(world, ae.task, ac);
if (aB.size() != 1 || aB.front().stage != aY::cQ) {
return std::nullopt;
}
return aB.front();
}
struct aT {
ag az;
double bb;
std::uint64_t order;
std::size_t within_order;
std::optional<std::size_t> target;
bool candidate = false;
};
std::vector<aT> bZ(
const ab& world, af au) {
std::vector<aT> result;
for (const bQ& future : futureTransfers(world.links, au)) {
result.push_back({
future.az, future.bb, future.trigger_order, future.dv, std::nullopt, false,
});
}
return result;
}
std::vector<double> ca(
const ab& world,
af au,
std::vector<aT> aB,
std::size_t target_count,
bool include_candidate,
const ai& system) {
std::sort(aB.begin(), aB.end(), [](const aT& left, const aT& right) {
return std::tie(left.bb, left.order, left.within_order)
< std::tie(right.bb, right.order, right.within_order);
});
std::vector<double> completions(target_count, -1.0);
double tail = directionState(world.links, au).bN;
for (const aT& projected : aB) {
if (projected.candidate && !include_candidate) {
continue;
}
tail = std::max(tail, projected.bb) + transferTime(system, projected.az.length);
if (projected.target.has_value()) {
completions.at(*projected.target) = tail;
}
}
return completions;
}
std::optional<double> hotDeadline(
const ab& world, const ag& az, const ah& config) {
std::optional<double> deadline;
for (const int rid : az.rids) {
const cw& request = ap(world, rid);
if (aD(request) != aj::cS || !request.aA.has_value()) {
continue;
}
const double request_deadline = *request.aA + config.system.SLO2;
deadline = std::min(deadline.value_or(request_deadline), request_deadline);
}
return deadline;
}
double cb(
const std::vector<double>& cm,
const std::vector<double>& bF,
const std::vector<double>& bi,
const ah& config) {
assert(cm.size() == bF.size());
assert(bF.size() == bi.size());
double maximum = 0.0;
for (std::size_t index = 0; index < bi.size(); ++index) {
const bool candidate_causes_delay = bF[index] > cm[index] + 1e-9;
if (candidate_causes_delay && bF[index] > bi[index]) {
const double aA = bi[index] - config.system.SLO2;
maximum = std::max(maximum, (bF[index] - aA) / config.system.SLO2);
}
}
return maximum;
}
std::optional<ad> cc(
const ab& world, const ah& config) {
std::vector<int> ready;
for (std::size_t rid = 0; rid < world.aw.size(); ++rid) {
const cw& request = world.aw[rid];
if (request.state == aa::ay && aD(request) == aj::cS) {
ready.push_back(static_cast<int>(rid));
}
}
if (ready.empty()) {
return std::nullopt;
}
std::sort(ready.begin(), ready.end(), [&](int left, int right) {
return cv(priorityOf(world, left), priorityOf(world, right));
});
const aq& fallback = config.multiprocessor.adaptive.bv;
ready.resize(static_cast<std::size_t>(
aI(fallback, aa::ay, ready.size())));
const ad bL{bt{bl::Edge, -1}, aM{std::move(ready)}};
return bx(world, bL, config);
}
double cd(
const ab& world,
const ad& candidate,
const ad& ba,
const ah& config) {
const ag ci = *cg(
world, candidate, config.system.ac);
const std::vector<ag> cj =
bp(world, ba.task, config.system.ac);
const double ck = world.aG + config.system.S
+ bs(world, candidate.task, config.curves, config.system.ac);
const double cl = config.system.S
+ bs(world, ba.task, config.curves, config.system.ac);
std::vector<aT> without = bZ(world, af::Up);
std::vector<aT> with = without;
with.push_back({
ci, ck, world.links.bI, 0, std::nullopt, true,
});
std::vector<double> bi;
bi.reserve(cj.size());
for (std::size_t index = 0; index < cj.size(); ++index) {
const ag& az = cj[index];
bi.push_back(*hotDeadline(world, az, config));
without.push_back({
az, world.aG + cl, world.links.bI, index, index, false,
});
with.push_back({
az, ck + cl, world.links.bI + 1, index, index, false,
});
}
const std::vector<double> without_completion = ca(
world, af::Up, std::move(without), bi.size(), true, config.system);
const std::vector<double> with_completion = ca(
world, af::Up, std::move(with), bi.size(), true, config.system);
return cb(without_completion, with_completion, bi, config);
}
double ce(
const ab& world,
std::size_t ch,
const std::vector<ad>& candidates,
const ah& config) {
std::vector<aT> projection = bZ(world, af::Down);
std::vector<double> bi;
for (std::size_t assignment_index = 0; assignment_index < candidates.size(); ++assignment_index) {
const ad& ae = candidates[assignment_index];
const std::vector<ag> aB =
bp(world, ae.task, config.system.ac);
const double bb = world.aG + config.system.S
+ bs(world, ae.task, config.curves, config.system.ac);
for (std::size_t within = 0; within < aB.size(); ++within) {
const ag& az = aB[within];
if (az.au != af::Down) {
continue;
}
const bool is_candidate = assignment_index == ch;
std::optional<std::size_t> target;
if (!is_candidate && az.stage == aY::Decode) {
const std::optional<double> deadline = hotDeadline(world, az, config);
if (deadline.has_value()) {
target = bi.size();
bi.push_back(*deadline);
}
}
projection.push_back({
az,
bb,
world.links.bI + assignment_index,
within,
target,
is_candidate,
});
}
}
if (bi.empty()) {
return 0.0;
}
const std::vector<double> cm = ca(
world, af::Down, projection, bi.size(), false, config.system);
const std::vector<double> bF = ca(
world, af::Down, std::move(projection), bi.size(), true, config.system);
return cb(cm, bF, bi, config);
}
int prefillRid(const ad& ae) {
if (const aH* task = std::get_if<aH>(&ae.task)) {
return task->rid;
}
const aL* task = std::get_if<aL>(&ae.task);
assert(task != nullptr);
return task->rid;
}
bool cf(
const ab& world,
std::size_t ch,
const std::vector<ad>& candidates,
const std::optional<ad>& ba,
const ah& config) {
const ad& candidate = candidates[ch];
const std::optional<ag> az =
cg(world, candidate, config.system.ac);
if (!az.has_value()) {
return false;
}
double cn = 0.0;
if (az->au == af::Up) {
if (!config.features.prefill_up_admission || !ba.has_value()) {
return false;
}
cn = cd(world, candidate, *ba, config);
} else {
if (!config.features.prefill_down_admission) {
return false;
}
cn = ce(world, ch, candidates, config);
}
const cw& prefill = ap(world, prefillRid(candidate));
const double co = (world.aG - prefill.by) / config.system.SLO1;
return cn > 1.0 && cn > co;
}
}
ah bX(
const ai& system,
const ax& curves,
bT features) {
return {
.multiprocessor = bK(system, curves),
.system = system,
.curves = curves,
.features = features,
};
}
std::vector<ag> bY(
const ab& world, const cy& task, int ac) {
return bp(world, task, ac);
}
double bs(
const ab& world, const cy& task, const ax& curves, int ac) {
assert(ac > 0);
return std::visit(bS{
[&](const aH& value) {
return aF(curves.cT, ap(world, value.rid).bg);
},
[&](const aL& value) {
const double full = aF(curves.cU, ap(world, value.rid).bg);
return full * (value.dk - value.dj) / ac;
},
[&](const aZ& value) {
return aF(curves.cV, ap(world, value.rid).bg);
},
[&](const aM& value) {
return aF(curves.cW, static_cast<int>(value.rids.size()));
},
[&](const be& value) {
return aF(curves.bB, static_cast<int>(value.rids.size()));
},
[&](const bf& value) {
return aF(curves.cX, static_cast<int>(value.rids.size()));
},
}, task);
}
void bU(
ab& world,
const ad& ae,
const ai& system,
const ax& curves) {
std::vector<ag> aB =
bp(world, ae.task, system.ac);
if (aB.empty()) {
return;
}
const double du = world.aG + system.S
+ bs(world, ae.task, curves, system.ac);
world.links.bq.push_back({
ae.server,
ae.task,
du,
world.links.bI++,
std::move(aB),
});
}
void bV(ab& world, const cA& frame, const ai& system) {
for (const cz& event : frame.events) {
if (const cC* done = std::get_if<cC>(&event)) {
const auto pending = std::find_if(
world.links.bq.begin(),
world.links.bq.end(),
[&](const bO& trigger) {
return sameServer(trigger.server, done->server) && sameTask(trigger.task, done->task);
});
std::vector<ag> aB;
if (pending != world.links.bq.end()) {
aB = std::move(pending->aB);
world.links.bq.erase(pending);
} else {
aB = bp(world, done->task, system.ac);
}
for (const ag& az : aB) {
commitTransfer(world.links, az, frame.timestamp, system);
}
} else if (const aS* transfer_done = std::get_if<aS>(&event)) {
reconcileTransfer(world.links, *transfer_done, frame.timestamp, system);
}
}
}
double bM(
const ab& world,
const std::vector<int>& rids,
const ah& config) {
assert(!rids.empty());
std::vector<int> counts(world.clouds.size(), 0);
for (const int rid : rids) {
const cw& request = ap(world, rid);
assert(request.remote.has_value());
++counts.at(static_cast<std::size_t>(*request.remote));
}
const double bb = world.aG + config.system.S
+ aF(config.curves.cW, static_cast<int>(rids.size()));
const double tail = tailBefore(
world.links, af::Up, bb, config.system);
double cost = std::max(0.0, tail - bb);
for (const int count : counts) {
if (count > 0) {
cost += transferTime(config.system, count);
}
}
return cost;
}
ad bx(
const ab& world,
const ad& bL,
const ah& config) {
const aM* baseline_task = std::get_if<aM>(&bL.task);
if (!config.features.decode_locality || baseline_task == nullptr || baseline_task->rids.size() < 2) {
return bL;
}
std::vector<bool> in_baseline(world.aw.size(), false);
for (const int rid : baseline_task->rids) {
in_baseline.at(static_cast<std::size_t>(rid)) = true;
}
std::vector<int> fillers;
for (std::size_t rid = 0; rid < world.aw.size(); ++rid) {
if (!in_baseline[rid] && world.aw[rid].state == aa::ay) {
fillers.push_back(static_cast<int>(rid));
}
}
std::sort(fillers.begin(), fillers.end(), [&](int left, int right) {
return cv(priorityOf(world, left), priorityOf(world, right));
});
std::vector<int> localized;
localized.reserve(baseline_task->rids.size());
std::vector<bool> ct(world.clouds.size(), false);
std::vector<bool> used(world.aw.size(), false);
for (const int cq : baseline_task->rids) {
const cw& original = ap(world, cq);
assert(original.remote.has_value());
const std::size_t original_remote = static_cast<std::size_t>(*original.remote);
int cs = cq;
if (!ct[original_remote] && !localized.empty()) {
const bR cr = priorityOf(world, cq);
const auto replacement = std::find_if(fillers.begin(), fillers.end(), [&](int cp) {
if (used.at(static_cast<std::size_t>(cp))) {
return false;
}
const cw& candidate = ap(world, cp);
assert(candidate.remote.has_value());
return ct.at(static_cast<std::size_t>(*candidate.remote))
&& cu(priorityOf(world, cp), cr, config);
});
if (replacement != fillers.end()) {
cs = *replacement;
}
}
const cw& selected = ap(world, cs);
ct.at(static_cast<std::size_t>(*selected.remote)) = true;
used.at(static_cast<std::size_t>(cs)) = true;
localized.push_back(cs);
}
if (bM(world, localized, config)
>= bM(world, baseline_task->rids, config)) {
return bL;
}
return {bL.server, aM{std::move(localized)}};
}
std::vector<ad> bW(
const ab& world, int ac, const ah& config) {
const std::vector<ad> bL =
chooseMultiprocessorAssignments(world, ac, config.multiprocessor);
if (!config.features.decode_locality
&& !config.features.prefill_up_admission
&& !config.features.prefill_down_admission) {
return bL;
}
std::vector<ad> candidates = bL;
for (ad& ae : candidates) {
ae = bx(world, ae, config);
}
std::vector<ad> accepted;
accepted.reserve(candidates.size());
const std::optional<ad> ba = cc(world, config);
bool added_protected_decode = false;
for (std::size_t index = 0; index < candidates.size(); ++index) {
const ad& ae = candidates[index];
if (!cf(world, index, candidates, ba, config)) {
accepted.push_back(ae);
continue;
}
const aH* cT = std::get_if<aH>(&ae.task);
if (cT != nullptr && ba.has_value() && !added_protected_decode) {
accepted.push_back(*ba);
added_protected_decode = true;
}
}
if (accepted.empty() && !bL.empty() && !hasGuaranteedFutureEvent(world)) {
return bL;
}
return accepted;
}
#include <cassert>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <variant>
#ifndef SCHEDULER_V7_STAGE
#define SCHEDULER_V7_STAGE 4
#endif
namespace {
bT df() {
constexpr int stage = SCHEDULER_V7_STAGE;
static_assert(stage >= 0 && stage <= 4);
switch (stage) {
case 0:
return {false, false, false};
case 1:
return {true, false, false};
case 2:
return {false, true, false};
case 3:
return {false, false, true};
default:
return {true, true, true};
}
}
}
int main() {
std::ios_base::sync_with_stdio(false);
std::cin.tie(nullptr);
ai config = cY(std::cin);
bd table = cZ(std::cin);
const ax curves = da(table);
const ah scheduler_config =
bX(config, curves, df());
ab world{config.K};
while (const auto frame = db(std::cin)) {
bV(world, *frame, config);
dc(world, *frame, config.ac);
const std::vector<ad> bm =
bW(world, config.ac, scheduler_config);
for (const ad& ae : bm) {
bU(world, ae, config, curves);
dd(world, ae, config.ac);
}
de(std::cout, bm);
std::cout << std::flush;
}
return 0;
}
