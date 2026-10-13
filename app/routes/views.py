from flask import Blueprint, render_template

views_bp = Blueprint("views", __name__)

@views_bp.route("/")
def dashboard():
    """Painel principal de gráficos, KPIs derivados e monitorização contínua."""
    return render_template("dashboard.html")

@views_bp.route("/records")
def records_manager():
    """Página de gestão, auditoria e exclusão granular de registos."""
    return render_template("records.html")