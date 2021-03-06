/**
 * \file io.cpp
 * \brief Implementations of I/O functions.
 * \copyright Copyright (c) 2016-2018, Barba group. All rights reserved.
 * \license BSD 3-Clause License.
 */


// STL
# include <fstream>
# include <sstream>
# include <vector>

// PETSc
# include <petscviewerhdf5.h>

// PetIBM
# include <petibm/io.h>


namespace petibm
{
namespace io
{
    
PetscErrorCode readLagrangianPoints(const std::string &file, 
        PetscInt &nPts, type::RealVec2D &coords)
{
    PetscFunctionBeginUser;

    std::ifstream   inFile(file);
    std::string     line;
    PetscInt        c = 0;
    PetscInt        dim = 0;

    // check if we successfully open the file
    if (! inFile.good()) SETERRQ1(PETSC_COMM_WORLD, PETSC_ERR_FILE_READ,
            "Opening or reading body file %s failed!", file.c_str());


    // read the total number of points
    if (std::getline(inFile, line)) 
    {
        std::stringstream   sline(line);

        if (! (sline >> nPts))
            SETERRQ1(PETSC_COMM_WORLD, PETSC_ERR_FILE_READ, 
                    "Can't read the total number of points in file %s !\n",
                    file.c_str());

        if (sline.peek() != EOF) 
            SETERRQ1(PETSC_COMM_WORLD, PETSC_ERR_FILE_READ, 
                    "The first line in file %s contains more than one integer. "
                    "Please check the format.\n", file.c_str());
    }
    else
        SETERRQ1(PETSC_COMM_WORLD, PETSC_ERR_FILE_READ,
                "Error while reading the first line in file %s !\n", file.c_str());

    
    // get the dimension from the first coordinate set; initialize coords
    {
        std::getline(inFile, line);
        std::stringstream   sline(line);
        PetscReal temp;
        while (sline >> temp) dim += 1;
    
        if (dim == 0)
            SETERRQ1(PETSC_COMM_WORLD, PETSC_ERR_FILE_READ,
                    "Could not calculate the dimension from the first coordinate "
                    "set in the file %s!\n", file.c_str());

        // initialize the size of coordinate array
        coords = type::RealVec2D(nPts, type::RealVec1D(dim, 0.0));
        
        // read again to get first coordinate set
        sline.str(line);
        sline.clear();
        for(PetscInt d=0; d<dim; ++d) sline >> coords[0][d];
        
        // increase c
        c += 1;
    }

    // loop through all points (from the second coordinate set)
    while(std::getline(inFile, line))
    {
        std::stringstream   sline(line);

        for(PetscInt d=0; d<dim; ++d)
            if (! (sline >> coords[c][d]))
                SETERRQ2(PETSC_COMM_WORLD, PETSC_ERR_FILE_READ,
                        "The number of doubles at line %d in file %s does not "
                        "match the dimension.\n", c+2, file.c_str());

        if (sline.peek() != EOF)
            SETERRQ2(PETSC_COMM_WORLD, PETSC_ERR_FILE_READ,
                        "The number of doubles at line %d in file %s does not "
                        "match the dimension.\n", c+2, file.c_str());

        c += 1;
    }

    // close the file
    inFile.close();

    // check the total number of points read
    if (c != nPts)
        SETERRQ1(PETSC_COMM_WORLD, PETSC_ERR_FILE_READ,
                "The total number of coordinates read in does not match the "
                "number specified at the first line in file %s !\n",
                file.c_str());

    PetscFunctionReturn(0);
} // readLagrangianPoints


PetscErrorCode print(const std::string &info)
{
    PetscFunctionBeginUser;
    
    PetscErrorCode  ierr;
    
    ierr = PetscSynchronizedPrintf(
            PETSC_COMM_WORLD, info.c_str()); CHKERRQ(ierr);
    
    ierr = PetscSynchronizedFlush(PETSC_COMM_WORLD, PETSC_STDOUT); CHKERRQ(ierr);
    
    ierr = PetscPrintf(PETSC_COMM_WORLD, "\n"); CHKERRQ(ierr);
    
    PetscFunctionReturn(0);
} // print


PetscErrorCode writeHDF5Vecs(const MPI_Comm comm, const std::string &file, 
        const std::string &loc, const std::vector<std::string> &names, 
        const std::vector<Vec> &vecs, const PetscFileMode mode)
{
    PetscFunctionBeginUser;
    
    PetscErrorCode  ierr;
    
    PetscViewer     viewer;
    
    // create viewer
    ierr = PetscViewerCreate(comm, &viewer); CHKERRQ(ierr);
    ierr = PetscViewerSetType(viewer, PETSCVIEWERHDF5); CHKERRQ(ierr);
    ierr = PetscViewerFileSetMode(viewer, mode); CHKERRQ(ierr);
    ierr = PetscViewerFileSetName(viewer, (file+".h5").c_str()); CHKERRQ(ierr);
    
    ierr = PetscViewerHDF5PushGroup(viewer, loc.c_str()); CHKERRQ(ierr);
    
    for(unsigned int i=0; i<vecs.size(); ++i)
    {
        ierr = PetscObjectSetName(
                (PetscObject) vecs[i], names[i].c_str()); CHKERRQ(ierr);
        
        ierr = VecView(vecs[i], viewer); CHKERRQ(ierr);
    }
    
    ierr = PetscViewerDestroy(&viewer); CHKERRQ(ierr);
    
    PetscFunctionReturn(0);
} // writeHDF5Vecs


PetscErrorCode writeHDF5Vecs(const MPI_Comm comm, const std::string &file,
        const std::string &loc, const std::vector<std::string> &names, 
        const std::vector<PetscInt> &n, const std::vector<PetscReal*> &vecs, 
        const PetscFileMode mode)
{
    PetscFunctionBeginUser;
    
    PetscErrorCode      ierr;
    PetscViewer         viewer;
    
    ierr = PetscViewerCreate(comm, &viewer); CHKERRQ(ierr);
    ierr = PetscViewerSetType(viewer, PETSCVIEWERHDF5); CHKERRQ(ierr);
    ierr = PetscViewerFileSetMode(viewer, mode); CHKERRQ(ierr);
    ierr = PetscViewerFileSetName(viewer, (file+".h5").c_str()); CHKERRQ(ierr);
    
    ierr = PetscViewerHDF5PushGroup(viewer, loc.c_str()); CHKERRQ(ierr);
    
    for(unsigned int i=0; i<vecs.size(); i++)
    {
        Vec     temp;
        ierr = VecCreateMPIWithArray(
                comm, 1, n[i], PETSC_DECIDE, nullptr, &temp); CHKERRQ(ierr);
        ierr = PetscObjectSetName(
                (PetscObject) temp, names[i].c_str()); CHKERRQ(ierr);
        ierr = VecPlaceArray(temp, vecs[i]); CHKERRQ(ierr);
        ierr = VecView(temp, viewer); CHKERRQ(ierr); 
        ierr = VecResetArray(temp); CHKERRQ(ierr);
        ierr = VecDestroy(&temp); CHKERRQ(ierr);
    }
    
    ierr = PetscViewerDestroy(&viewer); CHKERRQ(ierr);
    
    PetscFunctionReturn(0);
} // writeHDF5Vecs


PetscErrorCode writeHDF5Vecs(const MPI_Comm comm, const std::string &file,
        const std::string &loc, const std::vector<std::string> &names, 
        const type::RealVec2D &vecs, const PetscFileMode mode)
{
    PetscFunctionBeginUser;
    
    PetscErrorCode      ierr;
    PetscViewer         viewer;
    
    ierr = PetscViewerCreate(comm, &viewer); CHKERRQ(ierr);
    ierr = PetscViewerSetType(viewer, PETSCVIEWERHDF5); CHKERRQ(ierr);
    ierr = PetscViewerFileSetMode(viewer, mode); CHKERRQ(ierr);
    ierr = PetscViewerFileSetName(viewer, (file+".h5").c_str()); CHKERRQ(ierr);
    
    ierr = PetscViewerHDF5PushGroup(viewer, loc.c_str()); CHKERRQ(ierr);
    
    for(unsigned int i=0; i<vecs.size(); i++)
    {
        Vec     temp;
        ierr = VecCreateMPIWithArray(comm, 1, 
                vecs[i].size(), PETSC_DECIDE, nullptr, &temp); CHKERRQ(ierr);
        ierr = PetscObjectSetName(
                (PetscObject) temp, names[i].c_str()); CHKERRQ(ierr);
        ierr = VecPlaceArray(temp, vecs[i].data()); CHKERRQ(ierr);
        ierr = VecView(temp, viewer); CHKERRQ(ierr); 
        ierr = VecResetArray(temp); CHKERRQ(ierr);
        ierr = VecDestroy(&temp); CHKERRQ(ierr);
    }
    
    ierr = PetscViewerDestroy(&viewer); CHKERRQ(ierr);
    
    PetscFunctionReturn(0);
} // writeHDF5Vecs


PetscErrorCode readHDF5Vecs(const MPI_Comm comm, const std::string &file,
        const std::string &loc, const std::vector<std::string> &names, 
        std::vector<Vec> &vecs)
{
    PetscFunctionBeginUser;
    
    PetscErrorCode  ierr;
    
    PetscViewer     viewer;
    
    // create viewer
    ierr = PetscViewerCreate(comm, &viewer); CHKERRQ(ierr);
    ierr = PetscViewerSetType(viewer, PETSCVIEWERHDF5); CHKERRQ(ierr);
    ierr = PetscViewerFileSetMode(viewer, FILE_MODE_READ); CHKERRQ(ierr);
    ierr = PetscViewerFileSetName(viewer, (file+".h5").c_str()); CHKERRQ(ierr);
    
    ierr = PetscViewerHDF5PushGroup(viewer, loc.c_str()); CHKERRQ(ierr);
    
    for(unsigned int i=0; i<vecs.size(); ++i)
    {
        ierr = PetscObjectSetName(
                (PetscObject) vecs[i], names[i].c_str()); CHKERRQ(ierr);
        
        ierr = VecLoad(vecs[i], viewer); CHKERRQ(ierr);
    }
    
    ierr = PetscViewerDestroy(&viewer); CHKERRQ(ierr);
    
    PetscFunctionReturn(0);
} // readHDF5Vecs

} // end of namespace io
} // end of namespace petibm
