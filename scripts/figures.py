#!/usr/bin/env python3
"""Measured reward figure; no network access. Requires matplotlib/numpy."""
import os
from pathlib import Path
import json
import statistics
ROOT=Path(__file__).resolve().parents[1]
os.environ.setdefault('MPLCONFIGDIR',str(ROOT.parent/'tooling/matplotlib-cache'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

study=json.loads((ROOT/'docs/data/study.json').read_text())
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10,'axes.spines.top':False,'axes.spines.right':False,'figure.facecolor':'#f4f3ec','axes.facecolor':'#fcfcf7'})
fig,axes=plt.subplots(1,2,figsize=(10,4),sharey=True)
colors=['#226b53','#bd7020','#5669ad']
for ax,model in zip(axes,['Sonnet 4.6','gpt-6.1-sol']):
 for i,setup in enumerate(['solo','team3','iso3']):
  values=[r['reward'] for r in study['runs'] if r['hours']==3 and r['model']==model and r['setup']==setup]
  ax.scatter(np.linspace(i-.1,i+.1,len(values)),values,color=colors[i],s=25,alpha=.7,zorder=3)
  ax.errorbar(i,statistics.mean(values),yerr=statistics.stdev(values),color=colors[i],fmt='s',capsize=5,ms=7,zorder=4)
 ax.set(xticks=[0,1,2],xticklabels=['Solo','Team 3','Isolated 3'],ylim=(.65,1.01),title=model)
 ax.grid(axis='y',alpha=.2);ax.set_axisbelow(True)
axes[0].set_ylabel('Final graded reward')
fig.suptitle('TTC polyomino · 3 hours · 5 runs per configuration',fontsize=14)
fig.text(.5,.015,'Dots: individual runs. Squares and bars: mean ± sample standard deviation. Source: archived trial results.',ha='center',fontsize=8,color='#586a60')
fig.tight_layout(rect=(0,.05,1,.94))
out=ROOT.parent/'figures';out.mkdir(exist_ok=True)
fig.savefig(out/'final-rewards.png',dpi=180)
print('Figure written:',out/'final-rewards.png')
