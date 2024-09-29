#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Function to display usage
usage() {
  echo "Usage: $0 [patch|minor|major]"
  exit 1
}

# Check if an argument is provided
if [ $# -ne 1 ]; then
  usage
fi

INCREMENT_TYPE=$1

# Validate the argument
if [[ "$INCREMENT_TYPE" != "patch" && "$INCREMENT_TYPE" != "minor" && "$INCREMENT_TYPE" != "major" ]]; then
  echo "Invalid argument: $INCREMENT_TYPE"
  usage
fi

# Ensure 'jq' is installed for JSON manipulation
if ! command -v jq &> /dev/null; then
  echo "'jq' is not installed. Please install it to proceed."
  exit 1
fi

# Fetch the current version from plugin.json
CURRENT_VERSION=$(jq -r '.version' plugin.json)

# Check that CURRENT_VERSION is in the format MAJOR.MINOR.PATCH
if [[ ! $CURRENT_VERSION =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Current version '$CURRENT_VERSION' is not in MAJOR.MINOR.PATCH format."
  exit 1
fi

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

# Commit the changes
git add plugin.json
git commit -m "Bump version to $NEW_VERSION"

echo "Version bumped to $NEW_VERSION."
echo "Please merge this branch into 'main' and create a tag 'v$NEW_VERSION' on 'main' after merging."
