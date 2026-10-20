# SPDX-License-Identifier: MIT
require 'open3'
# A manual QMP quit can exit zero. Require the executed payload's marker too.
marker = ARGV.first&.start_with?("--marker=") ? ARGV.shift.delete_prefix("--marker=") : "MLIBC_ARM32_LOADER_OK"
output, status = Open3.capture2e(*ARGV)
print output
abort 'loader fixture did not complete its assertions' unless status.success? && output.include?(marker + "\n")
