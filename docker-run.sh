#!/bin/bash
# Docker management script for Dobrika Search Engine

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Detect docker compose command
if command -v docker-compose &> /dev/null; then
    DOCKER_COMPOSE="docker-compose"
elif docker compose version &> /dev/null; then
    DOCKER_COMPOSE="docker compose"
else
    echo "Error: docker compose not found"
    echo "Please install Docker Compose: https://docs.docker.com/compose/install/"
    exit 1
fi

show_help() {
    cat << EOF
Usage: ./docker-run.sh [COMMAND]

Commands:
    build       Build Docker image
    up          Start containers (build if needed)
    down        Stop and remove containers
    restart     Restart containers
    logs        Show container logs (follow mode)
    shell       Open shell in running container
    clean       Remove containers, images and volumes
    status      Show container status

Examples:
    ./docker-run.sh build
    ./docker-run.sh up
    ./docker-run.sh logs
EOF
}

case "${1:-}" in
    build)
        echo "Building Docker image..."
        $DOCKER_COMPOSE build
        ;;
    up)
        echo "Starting Dobrika Search Engine..."
        $DOCKER_COMPOSE up -d
        echo ""
        echo "✓ Server is running!"
        echo "  - API: http://localhost:8080"
        echo ""
        echo "View logs with: ./docker-run.sh logs"
        ;;
    down)
        echo "Stopping containers..."
        $DOCKER_COMPOSE down
        ;;
    restart)
        echo "Restarting containers..."
        $DOCKER_COMPOSE restart
        ;;
    logs)
        echo "Showing logs (Ctrl+C to exit)..."
        $DOCKER_COMPOSE logs -f
        ;;
    shell)
        echo "Opening shell in container..."
        $DOCKER_COMPOSE exec dobrika-search /bin/bash || \
        $DOCKER_COMPOSE run --rm dobrika-search /bin/bash
        ;;
    clean)
        echo "Cleaning up Docker resources..."
        $DOCKER_COMPOSE down -v
        docker rmi dobrika-search:latest 2>/dev/null || true
        echo "✓ Cleanup complete"
        ;;
    status)
        echo "Container status:"
        $DOCKER_COMPOSE ps
        ;;
    *)
        show_help
        exit 1
        ;;
esac

