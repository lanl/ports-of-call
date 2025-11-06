CHECKOUT_ROOT=$(git rev-parse --show-toplevel)

export XCAP_SPACKAGES_CHECKOUT="${CHECKOUT_ROOT}/extern/xcap_spackages"
export XCAP_SPACKAGES_REF="${XCAP_SPACKAGES_REF:-main}"

mkdir -p $(dirname ${XCAP_SPACKAGES_CHECKOUT})
REPO_URL=$(git remote get-url origin)

if [ ! -d "${XCAP_SPACKAGES_CHECKOUT}" ]; then
  git clone "${REPO_URL%/*/*}/spackages.git" "${XCAP_SPACKAGES_CHECKOUT}"
fi

git -C ${XCAP_SPACKAGES_CHECKOUT} checkout ${XCAP_SPACKAGES_REF}
