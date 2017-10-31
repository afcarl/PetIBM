/** Decoupled IBPM solver.
 * \file main.cpp
 */

#include <iomanip>
#include <sys/stat.h>

// PETSc
#include <petscsys.h>

// YAML-CPP
#include <yaml-cpp/yaml.h>

// PetIBM
#include <petibm/parser.h>

#include "decoupledibpm.h"


int main(int argc, char **argv)
{
    PetscErrorCode ierr;

    YAML::Node              config;
    petibm::type::Mesh      mesh;
    petibm::type::Boundary  bd;
    petibm::type::BodyPack  bodies;

    ierr = PetscInitialize(&argc, &argv, nullptr, nullptr); CHKERRQ(ierr);
    
    // get all settings and save into `config`
    ierr = petibm::parser::getSettings(config); CHKERRQ(ierr);

    // create mesh
    ierr = petibm::mesh::createMesh(PETSC_COMM_WORLD, config, mesh); CHKERRQ(ierr);
    
    // output the data of the mesh
    // TODO: ierr = mesh.write(params.caseDir, "grid"); CHKERRQ(ierr);
    
    // create bc
    ierr = petibm::boundary::createBoundary(mesh, config, bd); CHKERRQ(ierr);
    
    // create body pack
    ierr = petibm::body::createBodyPack(mesh, config, bodies); CHKERRQ(ierr);
    

    
    DecoupledIBPMSolver solver;

    // initialize the solver based on given mesh, boundary, bodies, and config
    ierr = solver.initialize(mesh, bd, bodies, config); CHKERRQ(ierr);

    

    // starting step
    PetscInt start = config["parameters"]["startStep"].as<PetscInt>();
    
    // end step
    PetscInt end = start + config["parameters"]["nt"].as<PetscInt>();
    
    // number of steps to save solutions
    PetscInt nsave = config["parameters"]["nsave"].as<PetscInt>();
    
    // number of steps to save solutions
    PetscInt nrestart = config["parameters"]["nrestart"].as<PetscInt>();

    // time-step size
    PetscReal dt = config["parameters"]["dt"].as<PetscReal>();

    // current time
    PetscReal t = start * dt;
             
    // file to log information of linear solvers
    std::string iterationsFile = 
        config["directory"].as<std::string>() + "/iterations.txt";
             
    // file to log averaged Lagrangian forces
    std::string forceFile = 
        config["directory"].as<std::string>() + "/forces.txt";
    
    
    if (start == 0) // write initial solutions to a HDF5
    {
        ierr = PetscPrintf(PETSC_COMM_WORLD,
                "[time-step 0] Writing solution... "); CHKERRQ(ierr);
        
        std::stringstream ss;
        ss << "/" << std::setfill('0') << std::setw(7) << 0;
        ierr = solver.write(
                (config["solution"].as<std::string>() + ss.str()));
        CHKERRQ(ierr);
        
        ierr = PetscPrintf(PETSC_COMM_WORLD, "done\n"); CHKERRQ(ierr);
    }
    else // restart
    {
        ierr = PetscPrintf(PETSC_COMM_WORLD,
                "[time-step %d] Read solution... ", start); CHKERRQ(ierr);
        
        std::stringstream ss;
        ss << "/" << std::setfill('0') << std::setw(7) << start;
        ierr = solver.readRestartData(
                (config["solution"].as<std::string>() + ss.str()));
        CHKERRQ(ierr);
        
        ierr = PetscPrintf(PETSC_COMM_WORLD, "done\n"); CHKERRQ(ierr);
    }
    
    // start time marching
    for (int ite=start+1; ite<=end; ite++)
    {
        // update current time
        t += dt;
        
        // advance one time-step
        ierr = solver.advance(); CHKERRQ(ierr);
        ierr = solver.writeIterations(ite, iterationsFile); CHKERRQ(ierr);
        
        if (ite % nsave == 0)
        {
            ierr = PetscPrintf(PETSC_COMM_WORLD,
                    "[time-step %d] Writing solution... ", ite); CHKERRQ(ierr);
            
            std::stringstream ss;
            ss << "/" << std::setfill('0') << std::setw(7) << ite;
            ierr = solver.write(
                    (config["solution"].as<std::string>() + ss.str())); 
            CHKERRQ(ierr);
            
            ierr = PetscPrintf(PETSC_COMM_WORLD, "done\n"); CHKERRQ(ierr);
        }
        
        if (ite % nrestart == 0)
        {
            ierr = PetscPrintf(PETSC_COMM_WORLD, 
                    "[time-step %d] Writing necessary data for restarting... ",
                    ite); CHKERRQ(ierr);
            
            std::stringstream ss;
            ss << "/" << std::setfill('0') << std::setw(7) << ite;
            ierr = solver.writeRestartData(
                    (config["solution"].as<std::string>() + ss.str()));
            CHKERRQ(ierr);
            
            ierr = PetscPrintf(PETSC_COMM_WORLD, "done\n"); CHKERRQ(ierr);
        }
        
        // write averaged force
        ierr = solver.writeIntegratedForces(t, forceFile); CHKERRQ(ierr);
    }

    ierr = PetscFinalize(); CHKERRQ(ierr);
    
    return 0;
} // main
