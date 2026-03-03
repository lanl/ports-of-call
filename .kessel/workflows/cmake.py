from kessel.workflows import *
from kessel.workflows.base.spack import BuildEnvironment
from kessel.workflows.base.cmake import CMake as CMakeWorkflow
from pathlib import Path


class Build(BuildEnvironment):
    steps = ["env", "configure", "build", "test", "install"]

    spack_env = environment("ports_of_call_default")
    project_spec = environment("ports-of-call+test")

    xcap_spackages_checkout = environment(Path.cwd() / "extern" / "xcap_spackages", variable="XCAP_SPACKAGES_CHECKOUT")

    def ci_message(self, args):
        pre_alloc_init = ""
        post_alloc_init = ""

        if "XCAP_SPACKAGES_MR" in self.environ and self.environ["XCAP_SPACKAGES_MR"]:
            pre_alloc_init += f"export XCAP_SPACKAGES_MR={self.environ['XCAP_SPACKAGES_MR']}\n"

        pre_alloc_init += "source .gitlab/download_prereqs.sh"

        if "DEPLOYMENT_VERSION" in self.environ and \
           "DEPLOYMENT_VERSION_CURRENT_DEFAULT" in self.environ and \
           self.environ["DEPLOYMENT_VERSION"] != self.environ["DEPLOYMENT_VERSION_CURRENT_DEFAULT"]:
            post_alloc_init += f"export DEPLOYMENT_VERSION={self.environ['DEPLOYMENT_VERSION']}\n"

        post_alloc_init += "source .gitlab/kessel.sh"
        return super().ci_message(args, pre_alloc_init=pre_alloc_init, post_alloc_init=post_alloc_init)

    @collapsed
    def env(self, args):
        """Prepare Environment"""
        self.xcap_spackages_checkout = self.source_dir / "extern" / "xcap_spackages"

        if "XCAP_SPACKAGES_MR" in self.environ and self.environ["XCAP_SPACKAGES_MR"]:
            self.allow_lockfile_changes = True

        if not self.xcap_spackages_checkout.exists():
            self.exec("source .gitlab/download_prereqs.sh")

        super().prepare_env(args)

        if "KESSEL_DEPLOYMENT" not in self.environ:
            self.exec(f"spack repo add {self.xcap_spackages_checkout}/v2/spack_repo/xcap || true")
            self.allow_lockfile_changes = True

        super().install_env(args)


class Cmake(Build, CMakeWorkflow):
    pass
