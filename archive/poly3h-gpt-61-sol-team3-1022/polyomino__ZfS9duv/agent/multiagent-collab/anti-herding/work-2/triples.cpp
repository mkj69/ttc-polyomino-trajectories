#define main nested_main
#include "../work-1/best96013.cpp"
#undef main

struct TripleInventory {
    struct Form {
        int type, r, f, w, h;
        long long minx, miny;
        string key;
    };
    struct GroupMember { int id; Placement placement; };
    struct ShapeType {
        vector<int> ids, forms;
    };
    vector<Piece> pieces;
    vector<vector<GroupMember>> members;
    vector<Form> forms;
    vector<ShapeType> types;
    vector<array<Placement, 8>> transform_cache;
    unordered_map<string, int> form_of;
    int grouped = 0;
    double preprocessing = 0;

    TripleInventory(const vector<Piece>& originals, double budget) {
        double begin = cpu();
        map<string, int> type_of;
        transform_cache.resize(originals.size());
        for (int id = 0; id < (int)originals.size(); ++id) {
            vector<pair<string, Placement>> keys;
            for (int f = 0; f < 2; ++f)
                for (int r = 0; r < 4; ++r) {
                    long long mx, my;
                    string key = normalize(originals[id], r, f, mx, my);
                    keys.push_back({key, {-mx, -my, r, f}});
                }
            string canonical = keys[0].first;
            for (auto& entry : keys) canonical = min(canonical, entry.first);
            int type;
            auto it = type_of.find(canonical);
            if (it == type_of.end()) {
                type = types.size();
                type_of[canonical] = type;
                types.push_back({});
                sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) {
                    return a.first < b.first;
                });
                string previous;
                for (auto& [key, placement] : keys) {
                    if (key == previous) continue;
                    previous = key;
                    Form form;
                    form.type = type;
                    form.r = placement.r;
                    form.f = placement.f;
                    form.minx = -placement.x;
                    form.miny = -placement.y;
                    form.w = form.h = 0;
                    form.key = key;
                    for (unsigned char code : key) {
                        form.w = max(form.w, code % 10 + 1);
                        form.h = max(form.h, code / 10 + 1);
                    }
                    int form_id = forms.size();
                    forms.push_back(move(form));
                    types[type].forms.push_back(form_id);
                    form_of[key] = form_id;
                }
            } else type = it->second;
            types[type].ids.push_back(id);
            for (int j = 0; j < (int)types[type].forms.size(); ++j) {
                const auto& key = forms[types[type].forms[j]].key;
                for (auto& entry : keys)
                    if (entry.first == key) {
                        transform_cache[id][j] = entry.second;
                        break;
                    }
            }
        }
        form_of.reserve(forms.size() * 2);
        vector<pair<int, int>> boxes{{5, 6}, {3, 10}, {4, 7}, {5, 5},
                                    {4, 6}, {3, 9}, {4, 5}, {3, 8}, {2, 10}};
        struct Record { uint32_t mask; int form, x, y; };
        struct Box {
            int w, h;
            uint32_t full;
            vector<vector<Record>> records;
        };
        vector<Box> box_records;
        for (auto [w, h] : boxes) {
            Box box{w, h, (1U << (w * h)) - 1, {}};
            box.records.resize(w * h);
            for (int form_id = 0; form_id < (int)forms.size(); ++form_id) {
                const auto& form = forms[form_id];
                if (form.w > w || form.h > h) continue;
                for (int y = 0; y + form.h <= h; ++y)
                    for (int x = 0; x + form.w <= w; ++x) {
                        uint32_t mask = 0;
                        for (unsigned char code : form.key)
                            mask |= 1U << ((code / 10 + y) * w + code % 10 + x);
                        int anchor = __builtin_ctz(mask);
                        box.records[anchor].push_back({mask, form_id, x, y});
                    }
            }
            for (auto& records : box.records)
                shuffle(records.begin(), records.end(), mt19937(817293));
            box_records.push_back(move(box));
        }
        vector<int> active(forms.size());
        iota(active.begin(), active.end(), 0);
        shuffle(active.begin(), active.end(), mt19937(817293));
        int attempts = 0;
        while (!active.empty() && cpu() - begin < budget) {
            int ia = active.size() - 1;
            int fa = active[ia];
            const auto& a = forms[fa];
            int ta = a.type;
            if (types[ta].ids.empty()) {
                active.pop_back();
                continue;
            }
            bool success = false;
            for (auto& box : box_records) {
                int w = box.w, h = box.h;
                if (cpu() - begin >= budget) break;
                if (a.w > w || a.h > h || (unsigned char)a.key[0] != 0) continue;
                uint32_t ma = 0;
                for (unsigned char code : a.key)
                    ma |= 1U << ((code / 10) * w + code % 10);
                int hole = __builtin_ctz(box.full ^ ma);
                for (auto& record : box.records[hole]) {
                ++attempts;
                if ((attempts & 4095) == 0 && cpu() - begin >= budget) break;
                int fb = record.form, tb = forms[fb].type;
                if (types[tb].ids.empty() || (tb == ta && types[tb].ids.size() < 2))
                    continue;
                if (ma & record.mask) continue;
                uint32_t complement = box.full ^ (ma | record.mask);
                int size = __builtin_popcount(complement);
                if (size < 1 || size > 10) continue;
                int minx = w, miny = h;
                for (uint32_t mask = complement; mask; mask &= mask - 1) {
                    int bit = __builtin_ctz(mask);
                    minx = min(minx, bit % w);
                    miny = min(miny, bit / w);
                }
                string key;
                for (uint32_t mask = complement; mask; mask &= mask - 1) {
                    int bit = __builtin_ctz(mask);
                    key.push_back(char((bit / w - miny) * 10 + bit % w - minx));
                }
                auto it = form_of.find(key);
                if (it == form_of.end()) continue;
                int fc = it->second, tc = forms[fc].type;
                int required = 1 + (tc == ta) + (tc == tb);
                if ((int)types[tc].ids.size() < required) continue;
                vector<GroupMember> group;
                auto take = [&](int type, int form_id, int x, int y) {
                    int id = types[type].ids.back();
                    types[type].ids.pop_back();
                    auto it = find(types[type].forms.begin(), types[type].forms.end(), form_id);
                    int orientation = it - types[type].forms.begin();
                    Placement placement = transform_cache[id][orientation];
                    placement.x += x;
                    placement.y += y;
                    group.push_back({id, placement});
                };
                take(ta, fa, 0, 0);
                take(tb, fb, record.x, record.y);
                take(tc, fc, minx, miny);
                Piece rectangle;
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x)
                        rectangle.cells.push_back({x, y});
                pieces.push_back(move(rectangle));
                members.push_back(move(group));
                grouped += 3;
                success = true;
                break;
                }
                if (success) break;
            }
            if (!success) {
                active.pop_back();
            }
        }
        for (auto& type : types)
            for (int id : type.ids) {
                pieces.push_back(originals[id]);
                members.push_back({{id, {0, 0, 0, 0}}});
            }
        preprocessing = cpu() - begin;
        if (getenv("PACK_DEBUG"))
            cerr << "triple grouped=" << grouped << '/' << originals.size()
                 << " attempts=" << attempts << " preprocessing=" << preprocessing << '\n';
    }

    Result expand(Result result, int n) const {
        vector<Placement> placements(n);
        for (int id = 0; id < (int)pieces.size(); ++id) {
            const auto& outer = result.placements[id];
            for (const auto& member : members[id]) {
                const auto& inner = member.placement;
                Point delta = transform({inner.x, inner.y}, outer.r, outer.f);
                int r = (outer.r + (outer.f ? -inner.r : inner.r) + 4) % 4;
                placements[member.id] = {outer.x + delta.x, outer.y + delta.y,
                                          r, outer.f ^ inner.f};
            }
        }
        result.placements = move(placements);
        return result;
    }
};

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n, total = 0;
    if (!(cin >> n)) return 0;
    vector<Piece> originals(n);
    for (auto& piece : originals) {
        int k;
        cin >> k;
        total += k;
        piece.cells.resize(k);
        for (auto& cell : piece.cells) cin >> cell.x >> cell.y;
    }
    double begin = cpu();
    double budget = argc > 2 ? atof(argv[2]) : 1.60;
    double preprocessing = argc > 3 ? atof(argv[3]) : .15;
    TripleInventory triples(originals, preprocessing);
    Catalogue original_catalogue(originals), grouped_catalogue(triples.pieces);
    vector<int> widths{40, 30, 50, 25, 32, 20, 16, 24, 36, 48};
    Result best;
    double last_cost = 0;
    int force = argc > 4 ? atoi(argv[4]) : -1;
    for (int i = 0; i < 10003; ++i) {
        double before = cpu();
        if (best.w && before + 1.3 * last_cost > begin + budget) break;
        int w = i < (int)widths.size() ? widths[i] :
            10 + rng() % max(2, min(75, int(sqrt(total) * 1.5)) - 9);
        if (argc > 1 && atoi(argv[1])) w = atoi(argv[1]);
        bool use_groups = triples.grouped && (force == 1 || (force < 0 && i % 2 == 0));
        const auto& catalogue = use_groups ? grouped_catalogue : original_catalogue;
        Packer packer(catalogue, w);
        Result trial;
        double difficulty = i % 3 == 0 ? 6 : i % 3 == 1 ? 10 : 12;
        if (n <= 500)
            trial = packer.nested(begin + budget, best.w ? best.area() : LLONG_MAX,
                                  .025, difficulty);
        else
            trial = packer.run(begin + budget, max(12, min(50, n / 4)),
                               best.w ? best.area() : LLONG_MAX, .012, difficulty);
        if (use_groups) trial = triples.expand(move(trial), n);
        last_cost = cpu() - before;
        if (!best.w || trial.area() < best.area()) best = move(trial);
        if (argc > 1 && atoi(argv[1])) break;
    }
    if (best.h > best.w) {
        int old_width = best.w;
        swap(best.w, best.h);
        for (auto& placement : best.placements) {
            long long x = placement.x;
            placement.x = placement.y;
            placement.y = old_width - 1 - x;
            placement.r = (placement.r + 1) % 4;
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (const auto& placement : best.placements)
        cout << placement.x << ' ' << placement.y << ' '
             << placement.r << ' ' << placement.f << '\n';
}
