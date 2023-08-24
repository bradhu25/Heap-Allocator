#!/usr/bin/python

from testing import *
import results


class Allocator(BaseTest):

    def score(self, student_ex, context):
        sanitized = student_ex.output.strip().replace("%", "%%")
        if student_ex.exitcode == 0 and "successfully" in student_ex.output:
            return results.Correct(short=sanitized)
        else:
            return results.Incorrect(short=sanitized)

class InstructionCount(BaseTest):

    core_cmd_expansion = "core_cmd /usr/bin/valgrind --tool=callgrind --toggle-collect=mymalloc --toggle-collect=myrealloc --toggle-collect=myfree --callgrind-out-file=/dev/null"

    def score(self, student_ex, context):
        if "ALLOCATOR FAILURE" in student_ex.output:
            return results.Incorrect(short="No instruction count, test_harness reported ALLOCATOR FAILURE")
        try:
            args = self.command.split(' ',1)[1].replace('-q', '')
            nrequests = int(util.system("grep '^[afr]' %s | wc -l" % args))
            refs = util.match_regex("I\s+refs:\s+((\d|,)+)", student_ex.output)
            per_req = int(ui.without_commas(refs))/nrequests
            utilization = int(util.match_regex("Utilization averaged (\d+)", student_ex.output))
        except:
            return results.Incorrect(short="Unable to scrape performance information")
        return results.Correct(short="Counted %s instructions for %d requests. %d instructions/request, utilization %d%%%%" % (refs, nrequests, per_req, utilization))


class CustomCount(InstructionCount):
    is_custom_template = True

    def init_from_string(self, line, num):
        self.name = "Custom-%d" % num
        command_str = line.strip()  # remove leading/trailing whitespace
        # if exec_name begins with ./  just discard (will allow either name or ./name as convenience)
        command_str = command_str[2:] if command_str.startswith("./") else command_str
        exec_name = command_str.split()[0]  # break off first token
        assert(exec_name in self.executables), "%s is not a valid executable choice, instead use one of %s" % (exec_name, ui.pretty_list(self.executables))
        self.command = "$" + command_str
        return self

    def execute_solution(self, path):
        pass
