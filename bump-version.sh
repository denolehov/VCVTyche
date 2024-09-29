#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Check if an argument (patch/minor/major) is provided
if [ $# -ne 1 ]; then
  echo "Usage: ./release.sh [patch|minor|major]"
  exit 1
fi

INCREMENT_TYPE=$1

# Validate the argument
if [[ "$INCREMENT_TYPE" != "patch" && "$INCREMENT_TYPE" != "minor" && "$INCREMENT_TYPE" != "major" ]]; then
  echo "Invalid argument. Use 'patch', 'minor', or 'major'."
  exit 1
fi

# Ensure 'jq' is installed for JSON manipulation
if ! command -v jq &> /dev/null; then
  echo "'jq' is not installed. Please install it to proceed."
  exit 1
fi

# Fetch the current version from plugin.json
CURRENT_VERSION=$(jq -r '.version' plugin.json)

# Split the version into major, minor, and patch
IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT_VERSION"

# Function to increment version numbers
increment_version() {
  case $1 in
    patch)
      PATCH=$((PATCH + 1))
      ;;
    minor)
      MINOR=$((MINOR + 1))
      PATCH=0
      ;;
    major)
      MAJOR=$((MAJOR + 1))
      MINOR=0
      PATCH=0
      ;;
  esac
}

# Increment the version
increment_version $INCREMENT_TYPE

# Form the new version string
NEW_VERSION="$MAJOR.$MINOR.$PATCH"

# Update the version in plugin.json
jq --arg version "$NEW_VERSION" '.version = $version' plugin.json > plugin.json.tmp && mv plugin.json.tmp plugin.json

# Commit the changes and create a new tag
git add plugin.json
git commit -m "Release version $NEW_VERSION"
git tag -a "v$NEW_VERSION" -m "Release version $NEW_VERSION"

# Push the commit and the tag to the repository
git push origin HEAD
git push origin "v$NEW_VERSION"

echo "Successfully released version $NEW_VERSION"
