-- Brian Chrzanowski
-- 2021-06-01 22:48:34

create table if not exists user
(
      id        text default (uuid())
    , ts        text default (strftime('%Y-%m-%dT%H:%M:%S', 'now'))

    , email     text not null
    , password  text not null

    , is_verified   int default (0)
);

create unique index if not exists idx_user_id on user (id);

create table if not exists exercise
(
      id        text default (uuid())
    , ts        text default (strftime('%Y-%m-%dT%H:%M:%S', 'now'))

    , kind      text not null
    , duration  int not null
);

create unique index if not exists idx_exercise_id on exercise (id);
create unique index if not exists idx_exercise_ts on exercise (ts);

create view if not exists v_tbl_exercise
as
select
      ts
    , kind
    , duration
from exercise;

create table if not exists sleep
(
      id        text default (uuid())
    , ts        text default (strftime('%Y-%m-%dT%H:%M:%S', 'now'))

    , hours     real not null
);

create view if not exists v_tbl_sleep
as
select
      ts
    , hours
from sleep;

create unique index if not exists idx_sleep_id on sleep (id);
create unique index if not exists idx_sleep_ts on sleep (ts);

create table if not exists bloodpressure
(
      id        text default (uuid())
    , ts        text default (strftime('%Y-%m-%dT%H:%M:%S', 'now'))

    , systolic  real not null
    , diastolic real not null
);

create unique index if not exists idx_bloodpressure_id on bloodpressure (id);
create unique index if not exists idx_bloodpressure_ts on bloodpressure (ts);

create view if not exists v_tbl_bloodpressure
as
select
      ts
    , systolic
    , diastolic
from bloodpressure;

create table if not exists meal
(
      id        text default (uuid())
    , ts        text default (strftime('%Y-%m-%dT%H:%M:%S', 'now'))

    , food      text not null
    , est_calories int not null
);

create unique index if not exists idx_meal_id on meal (id);
create unique index if not exists idx_meal_ts on meal (ts);

create view if not exists v_tbl_meal
as
select
      ts
    , food
    , est_calories
from meal;

create table if not exists weight
(
      id        text default (uuid())
    , ts        text default (strftime('%Y-%m-%dT%H:%M:%S', 'now'))

    , value     real not null
);

create unique index if not exists idx_weight_id on weight (id);
create unique index if not exists idx_weight_ts on weight (ts);

create view if not exists v_tbl_weight
as
select
      ts
    , value
from weight;

