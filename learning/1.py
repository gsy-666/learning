import execjs
with open("2.js",'r') as f:
  a=execjs.compile(f.read())
result=a.call('enc',"1000")
print(result)