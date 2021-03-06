###############################################################################
# Common test file for high priority DDL
###############################################################################
# in slave instance, DDL commands are executed on slave sql threads.
# Thus, these users aren't used for slave_high_priority_ddl
if (!$slave_ddl) {
  echo connection $con_init;
  connection $con_init;
  create user test_user1@localhost;
  grant all on test to test_user1@localhost;
  create user test_user2@localhost;
  grant all on test to test_user2@localhost;
}


--echo ##
--echo ## killing conflicting shared locks by alter table
--echo ##
--let $blocking_sql = lock tables t1 read;
--let $cmd = alter table t1 modify i bigint;
--let $high_priority_cmd = alter high_priority table t1 modify i bigint;
--source include/ddl_high_priority_module.inc


--echo ##
--echo ## killing conflicting shared lock in a transaction
--echo ## transaction will rollback
--echo ##
--let $blocking_sql = begin; insert into t1 values (4); select i from t1;
--let $cmd = alter table t1 rename t1_new;
--let $high_priority_cmd = alter high_priority table t1 rename t1_new;
--let $tablename = t1_new
--source include/ddl_high_priority_module.inc
select * from t1_new;
drop table t1_new;
# restore tablename
--let $tablename = t1


--echo ##
--echo ## simulate conflicting DDL which will not be killed
--echo ##
# Simulate conflicting DDL
# This will hold MDL_SHARED_NO_READ_WRITE, which may be upgraded to exclusive
# locks to run DDLs like ALTER TABLE
# the upgradable/exclusive lock should not be killed
--let $should_kill = 0
--let $should_table_exist = 0
--let $blocking_sql = lock tables t1 write;
--let $cmd = drop table t1;
--let $high_priority_cmd = drop high_priority table t1;
--source include/ddl_high_priority_module.inc
# restore $should_kill/$should_table_exist
--let $should_kill = 1
--let $should_table_exist = 1


--echo ##
--echo ## killing conflicting transaction by drop table DDL
--echo ##
--let $blocking_sql = lock tables t1 read; begin; insert into t1 values (4);
--let $cmd = drop table t1;
--let $high_priority_cmd = drop high_priority table t1;
--let $should_table_exist = 0
--source include/ddl_high_priority_module.inc
# restore $should_table_exist
--let $should_table_exist = 1

if (!$slave_ddl) {
  --echo ##
  --echo ## no effect for regular users
  --echo ##
  # save connected session count
  --source include/count_sessions.inc
  --let $begin_session_count =$count_sessions
  connect (con2, $con_block_host,test_user2,,test,$con_block_port,);
  # $con_kill is regular user
  --let $con_kill_orginal = $con_kill
  --let $con_kill = con2
  --let $should_kill = 0
  --let $blocking_sql = lock tables t1 read
  --let $cmd = alter table t1 modify i bigint

  --source include/ddl_high_priority_module.inc

  connection default;
  disconnect con2;

  # Ensure con2 is closed
  --let $count_sessions = $begin_session_count
  --source include/wait_until_count_sessions.inc

  # Secondary check to ensure con2 is closed
  let $wait_condition=
    SELECT count(*) = 1 FROM information_schema.processlist;
  --source include/wait_condition.inc

  # restore $con_kill
  --let $con_kill = $con_kill_orginal
  # restore $should_kill
  --let $should_kill = 1
}


--echo ##
--echo ## create index
--echo ##
--let $blocking_sql = lock tables t1 read;
--let $cmd = create index idx1 on t1 (i);
--let $high_priority_cmd = create high_priority index idx1 on t1 (i);
--source include/ddl_high_priority_module.inc


--echo ##
--echo ## drop index (use the previously created table)
--echo ##
--let $recreate_table = 0
--let $cmd = drop index idx1 on t1;
--let $high_priority_cmd = drop high_priority index idx1 on t1;
--source include/ddl_high_priority_module.inc
# restore $recreate_table
--let $recreate_table = 1


--echo ##
--echo ## high_priority truncate table
--echo ##
--let $blocking_sql = lock tables t1 read;
--let $cmd = truncate t1;
--let $high_priority_cmd = truncate high_priority t1;
--source include/ddl_high_priority_module.inc


--echo ##
--echo ## high_priority create/drop trigger
--echo ##
--let $blocking_sql = lock tables t1 read;
--let $cmd = create trigger ins_sum before insert on t1 for each row set @sum = @sum + new.i;
--let $high_priority_cmd = create high_priority trigger ins_sum before insert on t1 for each row set @sum = @sum + new.i;
--source include/ddl_high_priority_module.inc


--echo ##
--echo ## drop trigger (use the previously created table)
--echo ##
--let $recreate_table = 0
--let $cmd = drop trigger ins_sum;
--let $high_priority_cmd = drop high_priority trigger ins_sum;
--source include/ddl_high_priority_module.inc
# restore $recreate_table
--let $recreate_table = 1


--echo ##
--echo ## high_priority optimize table
--echo ##
--echo ## "optimize table" doesn't throw errors. It catches all errors, and
--echo ## returns a result set in a table
--echo ##
--let $throw_error = 0
--let $blocking_sql = lock tables t1 read;
--let $cmd = optimize table t1;
--let $high_priority_cmd = optimize high_priority table t1;
--source include/ddl_high_priority_module.inc
# restore throw_error
--let $throw_error = 1


if (!$slave_ddl) {
  --echo ##
  --echo ## high_priority lock tables
  --echo ##
  if ($use_sys_var) {
    --let $blocking_sql = lock tables t1 read;
    --let $cmd = lock tables t1 write;
    --source include/ddl_high_priority_module.inc
  }
}


--echo ##
--echo ## clean up
--echo ##
echo connection $con_init;
connection $con_init;
unlock tables;
if (!$slave_ddl) {
  drop user test_user1@localhost;
  drop user test_user2@localhost;
}
--disable_warnings
drop table if exists t1_new;
drop table if exists t1;
--enable_warnings
