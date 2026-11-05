# Preemptive priority with ageing: every 4 time units spent waiting raises priority by 1
jobs=[("P1",0,8,2),("P2",1,4,4),("P3",2,9,1),("P4",3,5,3)]
left={j[0]:j[2] for j in jobs}; waited={j[0]:0 for j in jobs}; t=0; g=[]
while any(left.values()):
    ready=[j for j in jobs if j[1]<=t and left[j[0]]>0]
    eff=lambda j: max(0, j[3]-waited[j[0]]//4)
    j=min(ready,key=lambda j:(eff(j),j[1]))
    for r in ready:
        if r is not j: waited[r[0]]+=1
    if g and g[-1][0]==j[0]: g[-1]=(j[0],g[-1][1],t+1)
    else: g.append((j[0],t,t+1))
    left[j[0]]-=1; t+=1
fin={};first={}
for n,s,e in g: first.setdefault(n,s); fin[n]=e
print("Gantt: "+" ".join(f"{n}({s}-{e})" for n,s,e in g))
for n,a,b,p in jobs: print(n,"TAT",fin[n]-a,"WT",fin[n]-a-b,"RT",first[n]-a)
print("avg WT",sum(fin[n]-a-b for n,a,b,p in jobs)/4)
