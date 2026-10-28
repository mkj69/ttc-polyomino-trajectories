struct ConflictSuffix {
    struct Pose {
        int x, y, oi;
        array<int, 10> cells{};
    };
    const vector<Piece>& pieces;
    const Result& prefix;
    Clock::time_point deadline;
    int width, height, bottom = 0, mode = 3, sampleSize = 256;
    int iterations = 0, bestEnergy = INT_MAX;
    vector<int> order, original;
    vector<vector<Pose>> domains;

    ConflictSuffix(const vector<Piece>& p, const Result& result, Clock::time_point d)
        : pieces(p), prefix(result), deadline(d), width(result.w), height(result.h - 1) {}

    Result ejectionSearch(int seed) {
        const int count = int(order.size()), cells = (height - bottom) * width;
        vector<int> owner(cells, -1), chosen = original, until(count), weight(count);
        vector<int> pending, bestChosen;
        randomState = 712367821ULL + uint64_t(seed) * 982451653ULL;
        auto put = [&](int at, int pose) {
            chosen[at] = pose;
            for (int k = 0; k < pieces[order[at]].area; ++k)
                owner[domains[at][pose].cells[k]] = at;
        };
        auto remove = [&](int at) {
            for (int k = 0; k < pieces[order[at]].area; ++k)
                owner[domains[at][chosen[at]].cells[k]] = -1;
            chosen[at] = -1;
            pending.push_back(at);
        };
        auto restore = [&] {
            fill(owner.begin(), owner.end(), -1);
            fill(until.begin(), until.end(), 0);
            pending.clear();
            chosen = bestChosen;
            for (int at = 0; at < count; ++at) {
                if (chosen[at] < 0) pending.push_back(at);
                else put(at, chosen[at]);
            }
        };
        for (int at = 0; at < count; ++at) {
            if (chosen[at] < 0) pending.push_back(at);
            else put(at, chosen[at]);
        }
        auto stateKey = [&] {
            int result = 100 * int(pending.size());
            for (int at : pending) result += pieces[order[at]].area;
            return result;
        };
        int bestKey = stateKey(), lastImprovement = 0;
        bestEnergy = int(pending.size());
        bestChosen = chosen;
        while (!pending.empty() && Clock::now() < deadline) {
            ++iterations;
            int slot = random32() % pending.size(), at = pending[slot];
            pending[slot] = pending.back();
            pending.pop_back();
            int size = int(domains[at].size()), next = -1, best = INT_MAX, ties = 0;
            int samples = mode & 1 ? size : min(size, sampleSize);
            for (int sample = 0; sample < samples; ++sample) {
                int pose = samples == size ? sample : int(random32() % size);
                array<U, 2> seen{};
                int collisions = 0, blocked = 0, value = 0;
                for (int k = 0; k < pieces[order[at]].area; ++k) {
                    int other = owner[domains[at][pose].cells[k]];
                    if (other < 0) continue;
                    ++collisions;
                    U bit = 1ULL << (other % 64);
                    if (seen[other / 64] & bit) continue;
                    seen[other / 64] |= bit;
                    ++blocked;
                    value += 10000 + ((mode & 2) ? weight[other] : 0);
                    if (until[other] > iterations) value += 50000;
                }
                value += 30 * collisions;
                if (blocked > bestEnergy + 3) continue;
                if (value < best) best = value, ties = 0;
                if (value == best && random32() % ++ties == 0) next = pose;
            }
            if (next < 0) {
                pending.push_back(at);
                restore();
                continue;
            }
            array<U, 2> colliders{};
            for (int k = 0; k < pieces[order[at]].area; ++k) {
                int other = owner[domains[at][next].cells[k]];
                if (other >= 0) colliders[other / 64] |= 1ULL << (other % 64);
            }
            for (int wi = 0; wi < 2; ++wi)
                for (U bits = colliders[wi]; bits; bits &= bits - 1) {
                    int other = wi * 64 + __builtin_ctzll(bits);
                    remove(other);
                    weight[other] += 60;
                }
            put(at, next);
            until[at] = iterations + 5 + random32() % 7;
            if (stateKey() < bestKey) {
                bestKey = stateKey();
                bestChosen = chosen;
                bestEnergy = int(pending.size());
                lastImprovement = iterations;
            }
            if (int(pending.size()) > bestEnergy + 3 ||
                iterations - lastImprovement > 512) {
                restore();
                lastImprovement = iterations;
            }
        }
        if (getenv("PACK_STATS"))
            cerr << "ejection count=" << count << " iterations=" << iterations
                 << " bestpending=" << bestEnergy << " finalpending=" << pending.size() << '\n';
        if (!pending.empty()) return {};
        Result result{width, height, prefix.placements};
        for (int at = 0; at < count; ++at) {
            const auto& pose = domains[at][chosen[at]];
            const auto& o = pieces[order[at]].orientations[pose.oi];
            result.placements[order[at]] = {pose.x - o.minx, pose.y - o.miny, o.r, o.f};
        }
        return result;
    }

    Result run(int count, int seed) {
        vector<int> orientation(pieces.size());
        vector<pair<int, int>> sequence;
        for (int id = 0; id < int(pieces.size()); ++id) {
            const auto& pose = prefix.placements[id];
            const auto& options = pieces[id].orientations;
            int oi = 0;
            while (oi < int(options.size()) &&
                   (options[oi].r != pose.r || options[oi].f != pose.f)) ++oi;
            if (oi == int(options.size())) abort();
            orientation[id] = oi;
            sequence.push_back({int(pose.y + options[oi].miny) + options[oi].h, id});
        }
        sort(sequence.rbegin(), sequence.rend());
        count = min<int>(count, pieces.size());
        vector<bool> removed(pieces.size());
        int removedArea = 0;
        bottom = height;
        for (int at = 0; at < count; ++at) {
            int id = sequence[at].second;
            order.push_back(id);
            removed[id] = true;
            removedArea += pieces[id].area;
            bottom = min(bottom, int(prefix.placements[id].y +
                                     pieces[id].orientations[orientation[id]].miny));
        }
        bottom = max(0, bottom - 3);
        vector<U> fixed(height, 0);
        for (int id = 0; id < int(pieces.size()); ++id) {
            if (removed[id]) continue;
            const auto& pose = prefix.placements[id];
            const auto& o = pieces[id].orientations[orientation[id]];
            int x = int(pose.x + o.minx), y = int(pose.y + o.miny);
            if (y + o.h > height) return {};
            for (int r = 0; r < o.h; ++r) fixed[y + r] |= o.row[r] << x;
        }
        int freeCells = 0;
        for (int y = bottom; y < height; ++y)
            freeCells += width - __builtin_popcountll(fixed[y]);
        if (freeCells < removedArea) return {};
        domains.resize(count);
        original.assign(count, -1);
        int domainCount = 0;
        for (int at = 0; at < count; ++at) {
            int id = order[at];
            const auto& p = pieces[id];
            for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
                const auto& o = p.orientations[oi];
                vector<pair<int, int>> offsets;
                for (int r = 0; r < o.h; ++r)
                    for (U bits = o.row[r]; bits; bits &= bits - 1)
                        offsets.push_back({__builtin_ctzll(bits), r});
                for (int y = bottom; y + o.h <= height; ++y) {
                    for (int x = 0; x + o.w <= width; ++x) {
                        bool fits = true;
                        for (int r = 0; r < o.h; ++r)
                            if (fixed[y + r] & (o.row[r] << x)) {
                                fits = false;
                                break;
                            }
                        if (!fits) continue;
                        Pose pose{x, y, oi, {}};
                        for (int cell = 0; cell < p.area; ++cell)
                            pose.cells[cell] = (y + offsets[cell].second - bottom) * width +
                                               x + offsets[cell].first;
                        if (oi == orientation[id] &&
                            x == prefix.placements[id].x + o.minx &&
                            y == prefix.placements[id].y + o.miny)
                            original[at] = int(domains[at].size());
                        domains[at].push_back(pose);
                    }
                }
            }
            if (domains[at].empty()) return {};
            domainCount += int(domains[at].size());
            if (Clock::now() >= deadline) return {};
        }
        if (mode >= 4) return ejectionSearch(seed);
        randomState = 712367821ULL + uint64_t(seed) * 982451653ULL;
        vector<int> occupancy((height - bottom) * width), weights(occupancy.size(), 1);
        vector<int> chosen(count, -1);
        vector<vector<int>> tabu(count);
        for (int at = 0; at < count; ++at) tabu[at].resize(domains[at].size());
        int energy = 0;
        auto update = [&](int at, int pose, int change) {
            for (int k = 0; k < pieces[order[at]].area; ++k) {
                int cell = domains[at][pose].cells[k];
                if (change > 0) energy += occupancy[cell]++;
                else energy -= --occupancy[cell];
            }
        };
        auto cost = [&](int at, int pose) {
            int value = 0;
            for (int k = 0; k < pieces[order[at]].area; ++k) {
                int cell = domains[at][pose].cells[k];
                value += occupancy[cell] * weights[cell];
            }
            return value;
        };
        for (int at = 0; at < count; ++at)
            if (original[at] >= 0) {
                chosen[at] = original[at];
                update(at, chosen[at], 1);
            }
        for (int at = 0; at < count; ++at) {
            if (chosen[at] >= 0) continue;
            int best = INT_MAX, ties = 0;
            for (int pose = 0; pose < int(domains[at].size()); ++pose) {
                int value = cost(at, pose);
                if (value < best) best = value, ties = 0;
                if (value == best && random32() % ++ties == 0) chosen[at] = pose;
            }
            update(at, chosen[at], 1);
        }
        bestEnergy = energy;
        int lastImprovement = 0;
        vector<int> conflicts;
        while (energy && Clock::now() < deadline) {
            ++iterations;
            conflicts.clear();
            for (int at = 0; at < count; ++at) {
                const auto& pose = domains[at][chosen[at]];
                for (int k = 0; k < pieces[order[at]].area; ++k)
                    if (occupancy[pose.cells[k]] > 1) {
                        conflicts.push_back(at);
                        break;
                    }
            }
            int at = conflicts[random32() % conflicts.size()];
            int previous = chosen[at], size = int(domains[at].size());
            update(at, previous, -1);
            int best = INT_MAX, next = previous, ties = 0;
            int samples = min(size, sampleSize);
            for (int sample = 0; sample < samples; ++sample) {
                int pose = samples == size ? sample : int(random32() % size);
                if (size > 1 && pose == previous) continue;
                if ((mode & 2) && tabu[at][pose] > iterations) continue;
                int value = cost(at, pose);
                if (value < best) best = value, ties = 0;
                if (value == best && random32() % ++ties == 0) next = pose;
            }
            if (random32() % 100 == 0) next = random32() % size;
            tabu[at][previous] = iterations + 7 + random32() % 7;
            chosen[at] = next;
            update(at, next, 1);
            if (energy < bestEnergy) {
                bestEnergy = energy;
                lastImprovement = iterations;
            }
            if ((mode & 1) && iterations - lastImprovement > 64 &&
                iterations % 64 == 0) {
                for (int cell = 0; cell < int(occupancy.size()); ++cell)
                    if (occupancy[cell] > 1) weights[cell] += 2;
            }
        }
        if (getenv("PACK_STATS"))
            cerr << "conflict count=" << count << " domains=" << domainCount
                 << " iterations=" << iterations << " bestenergy=" << bestEnergy
                 << " finalenergy=" << energy << '\n';
        if (energy) return {};
        Result result{width, height, prefix.placements};
        for (int at = 0; at < count; ++at) {
            const auto& pose = domains[at][chosen[at]];
            const auto& o = pieces[order[at]].orientations[pose.oi];
            result.placements[order[at]] = {pose.x - o.minx, pose.y - o.miny, o.r, o.f};
        }
        return result;
    }
};
