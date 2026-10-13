from flask import Blueprint, render_template

views_bp = Blueprint("views", __name__)

@views_bp.route("/")
def dashboard():
    """Renderiza a interface central com o painel reativo e os gráficos animados."""
    return render_template("dashboard.html")