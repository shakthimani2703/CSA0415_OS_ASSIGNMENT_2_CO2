# Scheduling calculator: FCFS, SJF, SRTF, Priority (non-preemptive), RR, MLFQ
import sys
def metrics(jobs, gantt):
    fin, first = {}, {}
    for n,s,e in gantt:
        first.setdefault(n, s); fin[n] = e
    rows=[]
    for n,a,b,p in jobs:
        tat=fin[n]-a; wt=tat-b; rt=first[n]-a
        rows.append((n,tat,wt,rt))
    return rows
def merge(g):
    out=[]
    for x in g:
        if out and out[-1][0]==x[0] and out[-1][2]==x[1]: out[-1]=(x[0],out[-1][1],x[2])
        else: out.append(x)
    return out
def nonpre(jobs,key):
    t=0; done=set(); g=[]
    while len(done)<len(jobs):
        ready=[j for j in jobs if j[1]<=t and j[0] not in done]
        if not ready: t=min(j[1] for j in jobs if j[0] not in done); continue
        j=min(ready,key=key); g.append((j[0],t,t+j[2])); t+=j[2]; done.add(j[0])
    return g
def srtf(jobs):
    left={j[0]:j[2] for j in jobs}; t=0; g=[]
    while any(left.values()):
        ready=[j for j in jobs if j[1]<=t and left[j[0]]>0]
        if not ready: t+=1; continue
        j=min(ready,key=lambda j:(left[j[0]],j[1])); g.append((j[0],t,t+1)); left[j[0]]-=1; t+=1
    return merge(g)
def ppri(jobs):
    left={j[0]:j[2] for j in jobs}; t=0; g=[]
    while any(left.values()):
        ready=[j for j in jobs if j[1]<=t and left[j[0]]>0]
        if not ready: t+=1; continue
        j=min(ready,key=lambda j:(j[3],j[1])); g.append((j[0],t,t+1)); left[j[0]]-=1; t+=1
    return merge(g)
def rr(jobs,q):
    left={j[0]:j[2] for j in jobs}; t=0; g=[]; Q=[jobs[0][0]]; added={jobs[0][0]}
    while Q:
        n=Q.pop(0); r=min(q,left[n]); g.append((n,t,t+r)); t+=r; left[n]-=r
        for j in jobs:
            if j[0] not in added and j[1]<=t: Q.append(j[0]); added.add(j[0])
        if left[n]>0: Q.append(n)
        if not Q:
            rest=[j for j in jobs if j[0] not in added]
            if rest: t=rest[0][1]; Q.append(rest[0][0]); added.add(rest[0][0])
    return g
def mlfq(jobs,quanta=(2,4,None)):
    # 3 levels: Q0 RR q=2, Q1 RR q=4, Q2 FCFS; preemption only at quantum end (new arrivals go to Q0)
    left={j[0]:j[2] for j in jobs}; lvl={}; t=0; g=[]; Qs=[[],[],[]]; added=set()
    def admit():
        for j in jobs:
            if j[0] not in added and j[1]<=t: Qs[0].append(j[0]); added.add(j[0]); lvl[j[0]]=0
    admit()
    while any(left.values()):
        L=next((i for i in range(3) if Qs[i]),None)
        if L is None: t=min(j[1] for j in jobs if j[0] not in added); admit(); continue
        n=Qs[L].pop(0); q=quanta[L] or left[n]
        r=0
        while r<q and left[n]>0:
            t+=1; r+=1; left[n]-=1; admit()
            if L>0 and Qs[0]: break        # higher-priority arrival preempts lower queue
        g.append((n,t-r,t))
        if left[n]>0:
            if r==q and L<2: Qs[L+1].append(n)
            else: Qs[L].append(n)
    return merge(g)
def show(title,jobs,g):
    rows=metrics(jobs,g); k=len(rows)
    print(f"## {title}")
    print("Gantt: "+" ".join(f"{n}({s}-{e})" for n,s,e in g))
    print("Proc  TAT  WT  RT")
    for r in rows: print(f"{r[0]:<5}{r[1]:>4}{r[2]:>4}{r[3]:>4}")
    print(f"Avg  {sum(r[1] for r in rows)/k:>5.2f} {sum(r[2] for r in rows)/k:>5.2f} {sum(r[3] for r in rows)/k:>5.2f}  switches={len(g)-1}")
    return rows
jobs=[("P1",0,8,2),("P2",1,4,4),("P3",2,9,1),("P4",3,5,3)]
show("FCFS",jobs,nonpre(jobs,lambda j:(j[1],)))
show("SJF (non-preemptive)",jobs,nonpre(jobs,lambda j:(j[2],j[1])))
show("SRTF",jobs,srtf(jobs))
show("Priority (non-preemptive, 1 = highest)",jobs,nonpre(jobs,lambda j:(j[3],j[1])))
show("Priority (preemptive)",jobs,ppri(jobs))
for q in (1,3,5,20): show(f"RR q={q}",jobs,merge(rr(jobs,q)))
show("MLFQ q=2,4,FCFS",jobs,mlfq(jobs))
print()
# video workload (seconds): one 20-min film arrives first, then thumbnails and a second film
vid=[("F1",0,1200,0),("T1",1,2,0),("T2",2,2,0),("T3",3,2,0),("F2",4,1200,0),("T4",5,2,0),("T5",6,2,0)]
def summary(title,g):
    rows=metrics(vid,g); s=[r for r in rows if r[0][0]=='T']; l=[r for r in rows if r[0][0]=='F']
    print(f"{title:<22} avgWT all={sum(r[2] for r in rows)/len(rows):7.1f}  thumbs WT={sum(r[2] for r in s)/len(s):7.1f} RT={sum(r[3] for r in s)/len(s):6.1f}  films WT={sum(r[2] for r in l)/len(l):7.1f}  switches={len(g)-1}")
print("## Video workload (F=film 1200 s, T=thumbnail 2 s)")
summary("FCFS",nonpre(vid,lambda j:(j[1],)))
summary("SJF",nonpre(vid,lambda j:(j[2],j[1])))
summary("SRTF",srtf(vid))
for q in (1,3,5,20): summary(f"RR q={q}",merge(rr(vid,q)))
summary("MLFQ q=2,4,FCFS",mlfq(vid))
summary("MLFQ q=4,16,FCFS",mlfq(vid,(4,16,None)))
