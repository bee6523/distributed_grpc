import sys

import redis

def create(source, answer):
    r = redis.Redis()
    dic = {}
    keys=[]
    vals=[]

    f = open(source, 'r')
    for l in f.readlines():
        l = ''.join(map(lambda x: ' ' if not x.isalnum() and x != ' ' else x, l))
        print(l)
        keys.extend(l[:-1].split())
    
    f = open(answer, 'r')
    for l in f.readlines():
        l = ''.join(map(lambda x: ' ' if not x.isalnum() and x != ' ' else x, l))
        print(l)
        vals.extend(l[:-1].split())

    for (k,v) in zip(keys, vals):
        k = ''.join(filter(str.isalnum, k))
        v = ''.join(filter(str.isalnum, v))
        dic[k] = v
    
    print(dic)
    r.mset(dic)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("python ./create_kv.py source answer")
    else:
        create(sys.argv[1], sys.argv[2])