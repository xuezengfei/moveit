/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2012, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Ioan Sucan, Mario Prats */

#include <ros/ros.h>
#include <moveit/benchmarks/benchmarks_config.h>
#include <moveit/benchmarks/benchmarks_utils.h>
#include <moveit/planning_scene/planning_scene.h>
#include <boost/program_options/variables_map.hpp>

static const std::string BENCHMARK_SERVICE_NAME = "benchmark_planning_problem";

static bool callBenchmarkService(ros::ServiceClient *service, bool run_planner, bool run_goal_existance,
                                 moveit_msgs::ComputePlanningPluginsBenchmark::Request &req)
{
  bool goal_ok = true;
  if (run_goal_existance)
  {
    ROS_INFO("Calling goal existance benchmark...");
    req.evaluate_goal_existance_only = true;
    std::string fnm_backup = req.filename;    
    if (!req.filename.empty())
    {
      bool changed = false;
      if (req.filename.length() > 3)
        if (req.filename.substr(req.filename.length() - 4) == ".log")
        {
          changed = true;
          req.filename = req.filename.substr(0, req.filename.length() - 4) + "_goal_only.log";
        }
      if (!changed)
        req.filename = req.filename + "_goal_only.log";
    }
    moveit_msgs::ComputePlanningPluginsBenchmark::Request res;
    if (service->call(req, res))
    {
      ROS_INFO("Success!");
    }
    else
    {
      goal_ok = false;
      ROS_ERROR("Failed");
    }
    req.filename = fnm_backup;
  }
  if (goal_ok && run_planner)
  {
    ROS_INFO("Calling planner benchmark...");
    req.evaluate_goal_existance_only = false;    
    moveit_msgs::ComputePlanningPluginsBenchmark::Request res;
    if (service->call(req, res))
    {
      ROS_INFO("Success!");
    }
    else
      ROS_ERROR("Failed");
  }
}

int main(int argc, char **argv)
{
  boost::program_options::options_description desc;
  desc.add_options()
    ("help", "Show help message")
    ("host", boost::program_options::value<std::string>(), "Host for the MongoDB.")
    ("port", boost::program_options::value<std::size_t>(), "Port for the MongoDB.")
    ("goal-existance-only", "Benchmark the sampling of the goal region")
    ("planners-only", "Benchmark only the planners");

  boost::program_options::variables_map vm;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
  boost::program_options::notify(vm);

  if (vm.count("help"))
  {
    std::cout << desc << std::endl;
    return 1;
  }

  bool planners_only = false;
  if (vm.count("planners-only"))
  {
    planners_only = true;
  }
  bool goal_existance_only = false;
  if (vm.count("goal-existance-only"))
  {
    goal_existance_only = true;
  }
  
  ros::init(argc, argv, "call_moveit_benchmark", ros::init_options::AnonymousName);
  ros::AsyncSpinner spinner(1);
  spinner.start();

  moveit_benchmarks::BenchmarkConfig bc(vm.count("host") ? vm["host"].as<std::string>() : "",
                                        vm.count("port") ? vm["port"].as<std::size_t>() : 0);

  std::vector<std::string> plugins = moveit_benchmarks::benchmarkGetAvailablePluginNames();

  if (plugins.empty())
    ROS_ERROR("There are no plugins to benchmark.");
  else
  {
    ros::NodeHandle nh;
    ros::service::waitForService(BENCHMARK_SERVICE_NAME);
    ros::ServiceClient benchmark_service_client = nh.serviceClient<moveit_msgs::ComputePlanningPluginsBenchmark>(BENCHMARK_SERVICE_NAME, true);
    moveit_benchmarks::BenchmarkCallFn fn = boost::bind(&callBenchmarkService, &benchmark_service_client, !goal_existance_only, !planners_only, _1);
    
    unsigned int proc = 0;
    for (int i = 1 ; i < argc ; ++i)
    {
      if (bc.readOptions(argv[i]))
      {
        std::stringstream ss;
        bc.printOptions(ss);
        ROS_INFO("Calling benchmark with options:\n%s\n", ss.str().c_str());
        bc.runBenchmark(fn);
        proc++;
      }
    }
    ROS_INFO("Processed %u benchmark configuration files", proc);
  }

  return 0;
}

