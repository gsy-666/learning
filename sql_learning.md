MySSQL
一.  关系型数据库（RDBMS)
  关系模型基础上，由多张相互连接的二维表组成的数据库
二. SQL语句：
  1. SQL语句单行或多行，分号结尾
  2. 缩进增加可读性
  3. MYSQL数据库的SQL语句不区分大小写，关键字建议大写
三、分类：
  DDL:定义语言，定义数据库对象
  DML：操作语言，增删改
  DQL：查询语言，查
  DCL：控制语言，创建数据库用户，控制数据库的访问权限
四、DDL
  1. 查询： SHOW DATABASES;
  2. 查询当前数据库： SELECT DATABASE();
  3. 创建：CREATE DATABASE [IF NOT EXISTS] 数据库名 [DEFAULT CHARSET 字符集] [COLLATE 排序规则]
  4. 删除： DROP DATABASE [IF EXISTS] 数据库名
  5. 使用： USE 数据库名


  -- 1. 创建数据库（带字符集，避免中文乱码）
        CREATE DATABASE IF NOT EXISTS db_test 
        DEFAULT CHARACTER SET utf8mb4  -- 支持emoji，比utf8更全
        DEFAULT COLLATE utf8mb4_general_ci;

  -- 2. 创建表（以用户表为例，覆盖常用字段类型）
        USE db_test;  -- 先切换到库
        CREATE TABLE IF NOT EXISTS user (
        id INT PRIMARY KEY AUTO_INCREMENT,  -- 主键+自增，唯一标识
        name VARCHAR(50) NOT NULL,          -- 字符串，非空
        age INT DEFAULT 0,                  -- 整数，默认值0
        create_time DATETIME DEFAULT NOW()  -- 时间，默认当前时间
        );

    -- 3. 删表/改表（常用）
        DROP TABLE IF EXISTS user;  -- 删除表
        ALTER TABLE user ADD phone VARCHAR(11);  -- 新增字段（手机号）
        ALTER TABLE user MODIFY phone CHAR(11);  -- 修改字段类型

五、DML （增删改（数据操作）
   1. 插入数据（新增）
INSERT INTO user (name, age, phone) 
VALUES ('张三', 25, '13800138000'),  -- 批量插入多条，用逗号分隔
       ('李四', 30, '13900139000');

   2. 修改数据（必加WHERE，否则改全表！）
UPDATE user SET age = 26 WHERE name = '张三';

   3. 删除数据（必加WHERE，否则删全表！）
DELETE FROM user WHERE id = 2;  -- 按主键删，最安全


六、DQL：查询
-- 基础结构：SELECT 字段 FROM 表 WHERE 条件 ORDER BY 字段 LIMIT 条数
-- 1. 查询所有字段（测试用，生产尽量指定字段）
SELECT * FROM user;

-- 2. 查询指定字段+条件
SELECT name, age FROM user WHERE age > 20;

-- 3. 常用条件/关键字
SELECT * FROM user 
WHERE name LIKE '张%'  -- 模糊查询（张开头）
AND age BETWEEN 20 AND 30  -- 年龄20-30
ORDER BY age DESC  -- 按年龄降序
LIMIT 10;  -- 只查前10条（分页常用）

-- 4. 聚合查询（统计）
SELECT COUNT(*) AS total,  -- 总条数
       MAX(age) AS max_age,  -- 最大年龄
       AVG(age) AS avg_age   -- 平均年龄
FROM user;


七、技巧：
1. 必避坑点：
    UPDATE/DELETE必须加WHERE，否则修改 / 删除全表数据；
    字符串 / 时间值要用单引号包裹（如name = '张三'）；
    主键尽量用INT AUTO_INCREMENT，保证唯一且自增。
2. 快速查错：
    语法错：看报错行号，检查关键字（如DATABASES/DATABASE、;结尾）；
    中文乱码：建库 / 建表时指定utf8mb4字符集；
3. 分页查询（高频）
    -- 分页公式：LIMIT (页码-1)*每页条数, 每页条数
                SELECT * FROM user LIMIT 0, 10;  -- 第1页，10条/页
                SELECT * FROM user LIMIT 10, 10; -- 第2页
4. 操作数据时，WHERE是保护线，UPDATE/DELETE必加，避免误操作；


  