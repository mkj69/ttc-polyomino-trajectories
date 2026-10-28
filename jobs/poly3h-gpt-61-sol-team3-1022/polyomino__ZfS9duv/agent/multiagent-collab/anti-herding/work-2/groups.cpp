#define main nested_main
#include "../work-1/best96013.cpp"
#undef main

struct RectangleInventory {
    struct Member { int id; Placement placement; };
    struct Form { int type, orientation, k, w, h, anchor; string key; };
    struct Record { uint64_t mask; int form, x, y; };
    struct Box {
        int w, h;
        uint64_t full;
        vector<vector<Record>> records;
        unordered_map<uint64_t, int> form_of;
    };
    vector<Piece> pieces;
    vector<vector<Member>> members;
    vector<vector<int>> ids;
    vector<array<Placement, 8>> transform_cache;
    vector<Form> forms;
    vector<int> counts;
    vector<Box> boxes;
    vector<Record> chosen;
    double deadline;
    int grouped = 0, group_size = 4;
    long long nodes = 0;
    bool expired = false;

    bool search(Box& box, uint64_t occupied, int left) {
        ++nodes;
        if ((nodes & 2047) == 0 && cpu() >= deadline) {
            expired = true;
            return false;
        }
        uint64_t free = box.full ^ occupied;
        int available = __builtin_popcountll(free);
        if (available < left || available > left * 10) return false;
        if (left == 1) {
            int minx = box.w, miny = __builtin_ctzll(free) / box.w;
            for (uint64_t mask = free; mask; mask &= mask - 1)
                minx = min(minx, __builtin_ctzll(mask) % box.w);
            auto it = box.form_of.find(free >> (miny * box.w + minx));
            if (it == box.form_of.end()) return false;
            int f = it->second, type = forms[f].type;
            if (!counts[type]) return false;
            --counts[type];
            chosen.push_back({free, f, minx, miny});
            return true;
        }
        int hole = __builtin_ctzll(free);
        for (const auto& record : box.records[hole]) {
            const auto& form = forms[record.form];
            if (!counts[form.type] || (record.mask & occupied)) continue;
            int rest = available - form.k;
            if (rest < left - 1 || rest > 10 * (left - 1)) continue;
            --counts[form.type];
            chosen.push_back(record);
            if (search(box, occupied | record.mask, left - 1)) return true;
            chosen.pop_back();
            ++counts[form.type];
            if (expired) return false;
        }
        return false;
    }

    RectangleInventory(const vector<Piece>& original, double budget, int size)
        : group_size(size) {
        double begin = cpu();
        deadline = begin + budget;
        map<string, int> type_of;
        transform_cache.resize(original.size());
        vector<vector<string>> type_keys;
        for (int id = 0; id < (int)original.size(); ++id) {
            vector<pair<string, Placement>> keys;
            for (int f = 0; f < 2; ++f)
                for (int r = 0; r < 4; ++r) {
                    long long mx, my;
                    string key = normalize(original[id], r, f, mx, my);
                    keys.push_back({key, {-mx, -my, r, f}});
                }
            sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) {
                return a.first < b.first;
            });
            const string& canonical = keys.front().first;
            auto it = type_of.find(canonical);
            int type;
            if (it == type_of.end()) {
                type = ids.size();
                type_of[canonical] = type;
                ids.push_back({});
                type_keys.push_back({});
                for (auto& [key, placement] : keys) {
                    if (!type_keys[type].empty() && type_keys[type].back() == key)
                        continue;
                    int orientation = type_keys[type].size();
                    Form form{type, orientation, (int)key.size(), 0, 0,
                              (unsigned char)key[0] % 10, key};
                    for (unsigned char code : key) {
                        form.w = max(form.w, code % 10 + 1);
                        form.h = max(form.h, code / 10 + 1);
                    }
                    forms.push_back(move(form));
                    type_keys[type].push_back(key);
                }
            } else type = it->second;
            ids[type].push_back(id);
            for (int orientation = 0; orientation < (int)type_keys[type].size(); ++orientation)
                for (auto& [key, placement] : keys)
                    if (key == type_keys[type][orientation]) {
                        transform_cache[id][orientation] = placement;
                        break;
                    }
        }
        for (const auto& copies : ids) counts.push_back(copies.size());
        vector<pair<int, int>> dimensions = group_size == 4 ?
            vector<pair<int, int>>{{5, 8}, {4, 10}, {6, 6}, {5, 7}, {4, 9}} :
            vector<pair<int, int>>{{5, 6}, {3, 10}, {4, 7}, {5, 5}, {4, 6}};
        for (auto [w, h] : dimensions) {
            Box box{w, h, (1ULL << (w * h)) - 1, {}, {}};
            box.records.resize(w * h);
            box.form_of.reserve(forms.size() * 2);
            for (int f = 0; f < (int)forms.size(); ++f) {
                const auto& form = forms[f];
                if (form.w > w || form.h > h) continue;
                uint64_t base = 0;
                for (unsigned char code : form.key)
                    base |= 1ULL << (code / 10 * w + code % 10);
                box.form_of[base] = f;
                for (int y = 0; y + form.h <= h; ++y)
                    for (int x = 0; x + form.w <= w; ++x) {
                        uint64_t mask = base << (y * w + x);
                        int hole = __builtin_ctzll(mask);
                        box.records[hole].push_back({mask, f, x, y});
                    }
            }
            for (auto& list : box.records)
                shuffle(list.begin(), list.end(), mt19937(172391));
            boxes.push_back(move(box));
        }
        vector<size_t> root_positions(boxes.size());
        bool progress = true;
        while (progress && cpu() < deadline) {
            progress = false;
            for (int b = 0; b < (int)boxes.size() && cpu() < deadline; ++b) {
                Box& box = boxes[b];
                auto& roots = box.records[0];
                while (root_positions[b] < roots.size()) {
                    const auto& root = roots[root_positions[b]++];
                    int type = forms[root.form].type;
                    if (!counts[type]) continue;
                    progress = true;
                    --counts[type];
                    chosen = {root};
                    if (search(box, root.mask, group_size - 1)) {
                        vector<Member> group;
                        for (const auto& record : chosen) {
                            const auto& form = forms[record.form];
                            int id = ids[form.type].back();
                            ids[form.type].pop_back();
                            Placement p = transform_cache[id][form.orientation];
                            p.x += record.x;
                            p.y += record.y;
                            group.push_back({id, p});
                        }
                        Piece rectangle;
                        for (int y = 0; y < box.h; ++y)
                            for (int x = 0; x < box.w; ++x)
                                rectangle.cells.push_back({x, y});
                        pieces.push_back(move(rectangle));
                        members.push_back(move(group));
                        grouped += group_size;
                        --root_positions[b];
                    } else {
                        ++counts[type];
                    }
                    break;
                }
                if (expired) break;
            }
            if (expired) break;
        }
        for (auto& copies : ids)
            for (int id : copies) {
                pieces.push_back(original[id]);
                members.push_back({{id, {0, 0, 0, 0}}});
            }
        if (getenv("PACK_DEBUG"))
            cerr << "group" << group_size << " grouped=" << grouped << '/'
                 << original.size() << " nodes=" << nodes
                 << " preprocessing=" << cpu() - begin << '\n';
    }

    Result expand(Result result, int n) const {
        vector<Placement> placements(n);
        for (int i = 0; i < (int)pieces.size(); ++i)
            for (const auto& member : members[i]) {
                const auto& outer = result.placements[i];
                const auto& inner = member.placement;
                Point delta = transform({inner.x, inner.y}, outer.r, outer.f);
                placements[member.id] = {
                    outer.x + delta.x, outer.y + delta.y,
                    (outer.r + (outer.f ? -inner.r : inner.r) + 4) % 4,
                    outer.f ^ inner.f
                };
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
    double budget = argc > 2 ? atof(argv[2]) : 1.6;
    RectangleInventory grouped(originals, argc > 3 ? atof(argv[3]) : .15,
                                argc > 5 ? atoi(argv[5]) : 4);
    Catalogue original_catalogue(originals), grouped_catalogue(grouped.pieces);
    Result best;
    double last_cost = 0;
    int force = argc > 4 ? atoi(argv[4]) : -1;
    for (int i = 0; i < 10003; ++i) {
        double before = cpu();
        if (best.w && before + 1.3 * last_cost > begin + budget) break;
        int upper = max(11, min(75, int(sqrt(total) * 1.5)));
        int w = i == 0 ? 40 : i == 1 ? 30 : 10 + rng() % (upper - 9);
        if (argc > 1 && atoi(argv[1])) w = atoi(argv[1]);
        bool use_groups = grouped.grouped && (force == 1 || (force < 0 && i % 2 == 0));
        Packer packer(use_groups ? grouped_catalogue : original_catalogue, w);
        Result trial;
        double difficulty = i % 3 == 0 ? 6 : i % 3 == 1 ? 10 : 12;
        if (n <= 500)
            trial = packer.nested(begin + budget, best.w ? best.area() : LLONG_MAX,
                                  .025, difficulty);
        else
            trial = packer.run(begin + budget, max(12, min(50, n / 4)),
                               best.w ? best.area() : LLONG_MAX, .012, difficulty);
        if (use_groups) trial = grouped.expand(move(trial), n);
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
