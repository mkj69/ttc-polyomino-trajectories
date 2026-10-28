mults = list(range(55, 401, 10))  # 0.55 to 4.00 by 0.10: 35 values
fine = list(range(85, 136, 2))    # 0.85 to 1.35 by 0.02: 26 values
all_m = sorted(set(mults + fine))
print(f"total mult values: {len(all_m)}")
# sq for n=10000, tot=55000
sq = 235
widths = sorted(set(max(1, int(sq*m/100)) for m in all_m))
print(f"distinct widths: {len(widths)}, range: {widths[0]}..{widths[-1]}")
print(f"3 sort orders × {len(widths)} widths = {3*len(widths)} runs")
