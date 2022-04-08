!  ========================================================================================
!  © (or copyright) 2019-2021. Triad National Security, LLC. All rights
!  reserved.  This program was produced under U.S. Government contract
!  89233218CNA000001 for Los Alamos National Laboratory (LANL), which is
!  operated by Triad National Security, LLC for the U.S.  Department of
!  Energy/National Nuclear Security Administration. All rights in the
!  program are reserved by Triad National Security, LLC, and the
!  U.S. Department of Energy/National Nuclear Security
!  Administration. The Government is granted for itself and others acting
!  on its behalf a nonexclusive, paid-up, irrevocable worldwide license
!  in this material to reproduce, prepare derivative works, distribute
!  copies to the public, perform publicly and display publicly, and to
!  permit others to do so.
!  ========================================================================================
#ifndef SINGLE_PRECISION_ENABLED
#define SINGLE_PRECISION_ENABLED 0
#endif

module ports_of_call
    use, intrinsic :: iso_c_binding
    implicit none

    public :: realkind
#if SINGLE_PRECISION_ENABLED
    integer, parameter :: realkind = c_float
#else
    integer, parameter :: realkind = c_double
#endif

end module ports_of_call