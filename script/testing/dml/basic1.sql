-- WARNING: DO NOT MODIFY THIS FILE
-- psql tests use the exact output from this file for CI

-- create the table
-- 2 columns, 1 primary key

drop table if exists foo;
create table foo(id integer PRIMARY KEY, year integer);
-- create index on foo (id); -- failed, why?

-- load in the data

insert into foo values(1, 100);
insert into foo values(1, 200); -- should fail
insert into foo values(2, 200);
insert into foo values(3, 300);
insert into foo values(4, 400);
insert into foo values(5, 400);
insert into foo values(5, 500); -- should fail

select * from foo order by id;

-- select

select * from foo where id < 3 order by id;

select * from foo where year > 200 order by id;

-- delete

delete from foo where year = 200;
select * from foo order by id;


-- -- update

update foo set year = 3000 where id = 3;
select * from foo order by id;

update foo set year = 1000 where year = 100; 
select * from foo order by id;

update foo set id = 3 where year = 1000; -- should fail
select * from foo order by id;

update foo set id= 10 where year = 1000;
select * from foo order by id;

-- insert again

insert into foo values (2, 2000);
select * from foo order by id;

insert into foo values (4, 4000); -- should fail
select * from foo order by id;

insert into foo values (1, 1000);
select * from foo order by id;

