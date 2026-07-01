<meta charset = "UTF-8">   字符集

a 超链接标签:
href:链接地址 - url地址
target:打开方式
  _blank:新窗口打开 
  _self:本窗口打开 (默认)

CSs引入方式:
  行内样式:写在标签的style属性中（配合JavaScript使用） <span style="color: gray;">2024年05月15日 20:07</span>
  内部样式:写在style标签中(可以写在页面任何位置，但通常约定写在head标签中)
  <style>span {
    color: gray;
}
</style>
  外部样式:写在一个单独的.css文件中(需要通过 link 标签在网页中引入)
