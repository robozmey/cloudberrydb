-- This is intended to test a new behavior of VACUUM AO/CO enhancement.
-- The enhacement introduced a new strategy to improve performance by
-- vacuuming indexes based on the collected AWAITING_DROP segment files,
-- instead of reading AO/CO visibility map catalog for every index tuple.
-- This behavior would lead to the index->reltuples being updated only when
-- AWAITING_DROP segment is greater than 0, which requires compaction during
-- VACUUM. If no compaction happens, even if dead tuples were deleted,
-- index->reltuples wouldn't get updated accordingly, which could generate
-- difference between table->reltuples and index->reltuples. That is supposed
-- to be fine in most cases since bloating size of indexes is limited in
-- the scope of gp_appendonly_compaction_threshold percentage of total tuples.
-- The new strategy would not impact table->reltuples updates.

create table vacuum_index_stats_pax (a int, b int, c int);
insert into vacuum_index_stats_pax select 2, b, b from generate_series(1, 11) b;
create index i_b_vacuum_index_stats_pax on vacuum_index_stats_pax(b);

set gp_appendonly_compaction_threshold = 10;
analyze vacuum_index_stats_pax;

-- expect reltuples == 11
0U: select reltuples from pg_class where relname = 'vacuum_index_stats_pax';
-- expect reltuples == 11
0U: select reltuples from pg_class where relname = 'i_b_vacuum_index_stats_pax';

-- delete one tuple
delete from vacuum_index_stats_pax where c = 1;
vacuum vacuum_index_stats_pax;

-- hideRatio = hiddenTupcount / totalTupcount * 100 = 1 / 11 * 100 = 9%
-- less than gp_appendonly_compaction_threshold (10%), no compaction would happen
-- during vacuum, expect no change in reltuples of the index but decrease 1 in
-- reltuples of the table.

-- expect reltuples == 10
0U: select reltuples from pg_class where relname = 'vacuum_index_stats_pax';
-- expect reltuples == 11 for no compaction happened
0U: select reltuples from pg_class where relname = 'i_b_vacuum_index_stats_pax';

analyze vacuum_index_stats_pax;

-- expect reltuples == 10
0U: select reltuples from pg_class where relname = 'vacuum_index_stats_pax';
-- expect reltuples == 10
0U: select reltuples from pg_class where relname = 'i_b_vacuum_index_stats_pax';

-- delete two tuples
delete from vacuum_index_stats_pax where c < 4;
vacuum vacuum_index_stats_pax;

-- hideRatio = hiddenTupcount / totalTupcount * 100 = 2 / 10 * 100 = 20%
-- greater than gp_appendonly_compaction_threshold (10%), compaction would happen
-- during vacuum, expect changes in reltuples for both index and table.

-- expect reltuples == 8
0U: select reltuples from pg_class where relname = 'vacuum_index_stats_pax';
-- expect reltuples == 8 for compaction happened
0U: select reltuples from pg_class where relname = 'i_b_vacuum_index_stats_pax';

0Uq:

drop table vacuum_index_stats_pax;
reset gp_appendonly_compaction_threshold;
